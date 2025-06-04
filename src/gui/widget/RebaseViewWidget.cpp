#include "gui/widget/RebaseViewWidget.h"

#include "action/Action.h"
#include "action/ActionManager.h"
#include "core/git/GitGraph.h"
#include "core/git/parser.h"
#include "core/git/types.h"
#include "core/state/CommandHistory.h"
#include "core/utils/todo.h"
#include "gui/color.h"
#include "gui/widget/CommitViewWidget.h"
#include "gui/widget/DiffWidget.h"
#include "gui/widget/graph/Graph.h"
#include "gui/widget/graph/Node.h"
#include "gui/widget/LineSplitter.h"
#include "gui/widget/ListItem.h"
#include "gui/widget/NamedListWidget.h"

#include <cassert>
#include <cstdint>
#include <ctime>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <git2/cherrypick.h>
#include <git2/commit.h>
#include <git2/index.h>
#include <git2/merge.h>
#include <git2/oid.h>
#include <git2/types.h>

#include <QAbstractItemModel>
#include <QBoxLayout>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QListWidgetItem>
#include <QMessageBox>
#include <qnamespace.h>
#include <QObject>
#include <QPalette>
#include <QSplitter>
#include <QWidget>

namespace gui::widget {

using action::Action;
using action::ActionType;

RebaseViewWidget::RebaseViewWidget(QWidget* parent)
    : QWidget(parent)
    , m_graph(core::git::GitGraph<Node*>::empty())
    , m_actions(action::ActionsManager::get()) {

    m_actions.clear();

    m_layout = new QHBoxLayout();
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);

    m_horizontal_split  = new LineSplitter(Qt::Orientation::Horizontal);
    m_left_split        = new LineSplitter(Qt::Orientation::Vertical);
    m_right_split       = new LineSplitter(Qt::Orientation::Horizontal);
    m_diff_commit_split = new LineSplitter(Qt::Orientation::Vertical);

    m_horizontal_split->addWidget(m_left_split);
    m_horizontal_split->addWidget(m_right_split);

    m_layout->addWidget(m_horizontal_split);

    m_list_actions = new NamedListWidget("Actions");
    m_graphs_split = new LineSplitter(Qt::Orientation::Horizontal);

    //-- LEFT LAYOUT --------------------------------------------------------//

    m_left_split->addWidget(m_list_actions);
    m_left_split->addWidget(m_graphs_split);

    m_old_commits_graph = new GraphWidget();
    m_new_commits_graph = new GraphWidget();

    m_graphs_split->addWidget(m_old_commits_graph);
    m_graphs_split->addWidget(m_new_commits_graph);

    //-- RIGHT LAYOUT -------------------------------------------------------//
    m_diff_widget = new DiffWidget();
    m_commit_view = new CommitViewWidget(m_diff_widget);

    m_right_split->addWidget(m_diff_commit_split);

    m_diff_commit_split->addWidget(m_diff_widget);
    m_diff_commit_split->addWidget(m_commit_view);

    m_diff_commit_split->setStretchFactor(0, 2);
    m_diff_commit_split->setStretchFactor(1, 0);

    connect(m_list_actions->getList(), &QListWidget::itemSelectionChanged, this, [this]() {
        if (m_last_item != nullptr) {
            m_last_item->setColorToAll(Qt::white);
        }

        QListWidget* list = m_list_actions->getList();
        auto selected     = list->selectedItems();
        if (selected.size() != 1) {
            m_last_item = nullptr;
            m_commit_view->update();
            return;
        }

        QListWidgetItem* item = selected.first();
        auto* list_item       = dynamic_cast<ListItem*>(m_list_actions->getList()->itemWidget(item));
        if (list_item == nullptr) {
            return;
        }
        m_last_item = list_item;

        list_item->setColorToAll(list_item->getItemColor());
        m_commit_view->update(m_last_item->getNode());
    });

    auto handle = [&](Node* prev, Node* next) { this->showCommit(prev, next); };

    m_old_commits_graph->setHandle(handle);
    m_new_commits_graph->setHandle(handle);

    m_list_actions->enableDrag();

    connect(
        m_list_actions->getList()->model(),
        &QAbstractItemModel::rowsMoved,
        this,
        [this](
            const QModelIndex& source, int source_row, int _end, const QModelIndex& _destination, int destination_row
        ) {
            assert(source_row == _end);
            assert(source == _destination);

            if (source_row == destination_row) {
                return;
            }

            if (source_row < destination_row) {
                destination_row -= 1;
            }

            core::state::CommandHistory::Add(
                std::make_unique<ListItemMoveCommand>(this, m_list_actions->getList(), source_row, destination_row)
            );

            this->moveAction(source_row, destination_row);

            this->updateGraph();

            this->m_commit_view->update(nullptr);
        }
    );
}

void RebaseViewWidget::moveAction(int from, int to) {
    assert(from >= 0 && to >= 0 && from != to);

    m_actions.move(from, to);
}

void RebaseViewWidget::showCommit(Node* prev, Node* next) {
    if (next == nullptr) {
        m_commit_view->update(nullptr);
        return;
    }

    if (prev == next) {
        return;
    }

    m_commit_view->update(next);
}

std::optional<std::string> RebaseViewWidget::update(
    git_repository* repo,
    const std::string& head,
    const std::string& onto,
    const std::vector<core::git::CommitAction>& actions
) {
    using core::git::CmdType;

    m_old_commits_graph->clear();
    m_actions.clear();

    m_repo = repo;

    auto graph_opt = core::git::GitGraph<Node*>::create(head.c_str(), onto.c_str(), repo);
    if (!graph_opt.has_value()) {
        return "Could not find commits";
    }

    m_graph = std::move(graph_opt.value());

    std::uint32_t max_depth = m_graph.max_depth();
    std::uint32_t max_width = m_graph.max_width();

    // TODO: Implement merge commits
    assert(max_width == 1);

    Node* parent = nullptr;
    std::string err_msg;
    m_graph.reverse_iterate([&](std::uint32_t depth, std::span<core::git::GitNode<Node*>> nodes) {
        auto y = max_depth - depth;

        for (auto& node : nodes) {
            auto* commit_node = m_old_commits_graph->addNode(y);
            commit_node->setCommit(node.commit);

            node.data = commit_node;
            node.data->setParentNode(parent);
            parent = commit_node;
        }
    });

    if (!err_msg.empty()) {
        return err_msg;
    }

    for (auto&& action : actions) {

        git_oid id;
        if (!core::git::get_oid_from_hash(id, action.hash.c_str(), m_repo)) {
            return "Could not find commit";
        }

        switch (action.type) {
        case CmdType::INVALID:
        case CmdType::NONE:
            TODO("Invalid command");
            break;
        case CmdType::PICK:
            m_actions.append(Action(ActionType::PICK, id, m_repo));
            break;
        case CmdType::REWORD:
            m_actions.append(Action(ActionType::REWORD, id, m_repo));
            break;
        case CmdType::EDIT:
            m_actions.append(Action(ActionType::EDIT, id, m_repo));
            break;
        case CmdType::SQUASH:
            m_actions.append(Action(ActionType::SQUASH, id, m_repo));
            break;
        case CmdType::FIXUP:
            m_actions.append(Action(ActionType::FIXUP, id, m_repo));
            break;
        case CmdType::DROP:
            m_actions.append(Action(ActionType::DROP, id, m_repo));
            break;
        case CmdType::LABEL:
        case CmdType::EXEC:
        case CmdType::BREAK:
        case CmdType::RESET:
        case CmdType::MERGE:
        case CmdType::UPDATE_REF:
            TODO("Unsupported command");
            break;
        }
    }

    return prepareActions();
}

std::optional<std::string> RebaseViewWidget::prepareActions() {

    prepareGraph();

    m_list_actions->clear();

    auto* list = m_list_actions->getList();
    for (auto& action : m_actions) {
        auto* action_item = new ListItem(this, list, list->count(), action);

        auto result = prepareItem(action_item, action);
        if (auto err = result) {
            return err;
        }
        auto* item = new QListWidgetItem();

        item->setSizeHint(action_item->sizeHint());
        list->addItem(item);
        list->setItemWidget(item, action_item);
    }

    return std::nullopt;
}

void RebaseViewWidget::prepareGraph() {
    m_last_item = nullptr;

    m_new_commits_graph->clear();

    auto* last      = m_new_commits_graph->addNode(0);
    auto& last_node = m_graph.first_node();
    m_root_node     = last_node.data;

    last->setCommit(last_node.commit);

    m_last_new_commit = last;
    m_commit_view->update();
}

void RebaseViewWidget::updateActions() {
    auto opt = prepareActions();

    if (opt.has_value()) {
        QMessageBox::critical(this, "Operation Error", opt.value().c_str());

        // TODO: Rollback
        return;
    }
}

void RebaseViewWidget::updateGraph() {
    prepareGraph();

    m_commit_view->update();

    auto* list = m_list_actions->getList();

    for (std::int32_t i = 0; i < list->count(); ++i) {
        auto* raw_item = list->itemWidget(list->item(i));
        auto* item     = dynamic_cast<ListItem*>(raw_item);
        assert(item != nullptr);

        item->clearConnections();

        Action& act = item->getCommitAction();

        switch (act.get_type()) {
        case ActionType::DROP: {
            item->setItemColor(convert_to_color(ColorType::DELETION));
            Node* old = findOldCommit(act.get_oid());
            if (old != nullptr) {
                item->addConnection(old);
                item->setNode(old);
            }
            break;
        }
        case ActionType::FIXUP:
        case ActionType::SQUASH: {
            item->setItemColor(convert_to_color(ColorType::INFO));

            Node* old = findOldCommit(act.get_oid());
            if (old != nullptr) {
                item->addConnection(old);
                updateNode(m_last_new_commit, m_last_new_commit, old);
            }

            item->addConnection(m_last_new_commit);
            item->setNode(m_last_new_commit);
            break;
        }

        case ActionType::PICK:
        case ActionType::REWORD:
        case ActionType::EDIT: {
            Node* old = findOldCommit(act.get_oid());
            if (old != nullptr) {
                item->addConnection(old);
            }
            item->setItemColor(convert_to_color(ColorType::ADDITION));

            Node* new_node = m_new_commits_graph->addNode();
            new_node->setCommit(act.get_commit());
            new_node->setParentNode(m_last_new_commit);
            new_node->setAction(&act);

            item->addConnection(new_node);
            item->setNode(new_node);

            updateNode(new_node, m_last_new_commit, new_node);

            m_last_new_commit = new_node;
            break;
        }
        }
    }
}

Node* RebaseViewWidget::findOldCommit(const git_oid& oid) {
    core::git::commit_t commit;

    if (git_commit_lookup(&commit, m_repo, &oid) != 0) {
        return nullptr;
    }

    return m_old_commits_graph->find([&](const Node* node) { return git_oid_equal(node->getCommitId(), &oid) != 0; });
}

void RebaseViewWidget::updateNode(Node* node, Node* current, Node* changes) {
    auto* current_commit = current->getCommit();
    auto* changes_commit = changes->getCommit();

    core::git::index_t index;

    if (git_cherrypick_commit(&index, m_repo, changes_commit, current_commit, 0, nullptr) != 0) {
        return;
    }

    if (git_index_has_conflicts(index) != 0) {
        node->setConflict(true);
    }
}

std::optional<std::string> RebaseViewWidget::prepareItem(ListItem* item, Action& action) {
    QString item_text;
    item_text += " [";
    item_text += QString::fromStdString(core::git::format_oid_to_str<7>(&action.get_oid()));
    item_text += "]: ";

    switch (action.get_type()) {
    case ActionType::DROP: {
        item->setItemColor(convert_to_color(ColorType::DELETION));

        Node* old = findOldCommit(action.get_oid());
        if (old != nullptr) {
            item->addConnection(old);
            item->setNode(old);
            item_text += git_commit_summary(old->getCommit());
        } else {
            item_text += git_commit_summary(action.get_commit());
        }
        break;
    }

    case ActionType::FIXUP:
    case ActionType::SQUASH: {
        item->setItemColor(convert_to_color(ColorType::INFO));

        Node* old = findOldCommit(action.get_oid());
        if (old != nullptr) {
            item->addConnection(old);
            updateNode(m_last_new_commit, m_last_new_commit, old);
            item_text += git_commit_summary(old->getCommit());
        } else {
            item_text += git_commit_summary(action.get_commit());
        }
        item->addConnection(m_last_new_commit);
        item->setNode(m_last_new_commit);
        break;
    }

    case ActionType::PICK:
    case ActionType::REWORD:
    case ActionType::EDIT: {
        Node* old = findOldCommit(action.get_oid());
        if (old != nullptr) {
            item->addConnection(old);
            item_text += git_commit_summary(old->getCommit());
        } else {
            item_text += git_commit_summary(action.get_commit());
        }
        item->setItemColor(convert_to_color(ColorType::ADDITION));

        Node* new_node = m_new_commits_graph->addNode();
        new_node->setCommit(action.get_commit());
        new_node->setParentNode(m_last_new_commit);
        new_node->setAction(&action);

        item->addConnection(new_node);
        item->setNode(new_node);

        updateNode(new_node, m_last_new_commit, new_node);

        m_last_new_commit = new_node;
        break;
    }
    }

    item->setText(item_text);

    auto* combo = item->getComboBox();

    connect(combo, &QComboBox::currentIndexChanged, this, [&](int index) {
        assert(index != -1);
        updateGraph();
    });

    return std::nullopt;
}

}
