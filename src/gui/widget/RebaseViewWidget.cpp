#include "gui/widget/RebaseViewWidget.h"

#include "core/git/GitGraph.h"
#include "core/git/parser.h"
#include "core/git/types.h"
#include "core/state/Command.h"
#include "core/state/CommandHistory.h"
#include "core/utils/todo.h"
#include "gui/widget/CommitViewWidget.h"
#include "gui/widget/DiffWidget.h"
#include "gui/widget/graph/Graph.h"
#include "gui/widget/graph/Node.h"
#include "gui/widget/ListItem.h"
#include "gui/widget/NamedListWidget.h"

#include <cassert>
#include <cstdint>
#include <ctime>
#include <git2/cherrypick.h>
#include <git2/commit.h>
#include <git2/index.h>
#include <git2/merge.h>
#include <git2/types.h>
#include <memory>
#include <optional>
#include <qboxlayout.h>
#include <qcolor.h>
#include <qlabel.h>
#include <QListWidgetItem>
#include <qnamespace.h>
#include <qobject.h>
#include <QPalette>
#include <QSplitter>
#include <QWidget>
#include <span>
#include <string>
#include <utility>
#include <vector>

class ListItemMoveCommand : public core::state::Command {
public:
    ListItemMoveCommand(RebaseViewWidget* rebase, QListWidget* parent, int prev_row, int curr_row)
        : m_rebase(rebase)
        , m_parent(parent)
        , m_prev(prev_row)
        , m_curr(curr_row) { }

    ~ListItemMoveCommand() override = default;

    void execute() override {
        move(m_prev, m_curr);
        m_rebase->updateActions();
    }

    void undo() override {
        move(m_curr, m_prev);
        m_rebase->updateActions();
    }

private:
    RebaseViewWidget* m_rebase;
    QListWidget* m_parent;
    int m_prev;
    int m_curr;

    void move(int from, int to) {
        auto* item = m_parent->takeItem(from);
        m_parent->insertItem(to, item);
    }
};

RebaseViewWidget::RebaseViewWidget(QWidget* parent)
    : QWidget(parent)
    , m_graph(GitGraph<Node*>::empty()) {

    m_layout = new QHBoxLayout();
    setLayout(m_layout);

    m_left_layout  = new QVBoxLayout();
    m_right_layout = new QVBoxLayout();

    m_layout->addLayout(m_left_layout, 1);
    m_layout->addLayout(m_right_layout, 2);

    m_list_actions  = new NamedListWidget("Actions");
    m_graphs_layout = new QHBoxLayout();

    //-- LEFT LAYOUT --------------------------------------------------------//
    m_left_layout->addWidget(m_list_actions);
    m_left_layout->addLayout(m_graphs_layout);

    m_old_commits_graph = new GraphWidget();
    m_new_commits_graph = new GraphWidget();

    m_graphs_layout->addWidget(m_old_commits_graph, 1);
    m_graphs_layout->addWidget(m_new_commits_graph, 1);

    //-- RIGHT LAYOUT -------------------------------------------------------//
    m_diff_widget = new DiffWidget();
    m_commit_view = new CommitViewWidget(m_diff_widget);

    m_right_layout->addWidget(m_diff_widget, 1);
    m_right_layout->addWidget(m_commit_view);

    connect(m_list_actions->getList(), &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (m_last_item != nullptr) {
            m_last_item->setColorToAll(Qt::white);
        }

        if (item != nullptr) {

            auto* list_item = dynamic_cast<ListItem*>(item);
            if (list_item == nullptr) {
                return;
            }
            m_last_item = list_item;

            list_item->setColorToAll(list_item->getItemColor());
            m_commit_view->update(m_last_item->getNode());
        }
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

            core::state::CommandHistory::Add(
                std::make_unique<ListItemMoveCommand>(this, m_list_actions->getList(), source_row, destination_row)
            );

            this->updateActions();
        }
    );
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
    git_repository* repo, const std::string& head, const std::string& onto, const std::vector<CommitAction>& actions
) {
    m_old_commits_graph->clear();
    m_repo = repo;

    auto graph_opt = GitGraph<Node*>::create(head.c_str(), onto.c_str(), repo);
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
    m_graph.reverse_iterate([&](std::uint32_t depth, std::span<GitNode<Node*>> nodes) {
        auto y = max_depth - depth;

        for (auto& node : nodes) {
            auto* commit_node = m_old_commits_graph->addNode(y);
            commit_node->setCommit(node.commit.commit);

            git_tree* tree;
            if (git_commit_tree(&tree, node.commit.commit) != 0) {
                err_msg = "Could not find commit tree";
                return;
            }

            commit_node->setGitTreeOwnership(tree);

            node.data = commit_node;
            node.data->setParentNode(parent);
            parent = commit_node;
        }
    });

    if (!err_msg.empty()) {
        return err_msg;
    }

    return prepareActions(actions);
}

std::optional<std::string> RebaseViewWidget::prepareActions(const std::vector<CommitAction>& actions) {
    m_new_commits_graph->clear();
    m_list_actions->clear();
    m_last_item = nullptr;

    auto* last      = m_new_commits_graph->addNode(0);
    auto& last_node = m_graph.first_node();
    m_root_node     = last_node.data;

    last->setCommit(last_node.commit.commit);
    last->setGitTree(last_node.data->getGitTree());

    m_last_new_commit = last;

    for (const auto& action : actions) {
        auto* action_item = new ListItem(m_list_actions->getList());

        QString text = cmd_to_str(action.type);
        auto result  = prepareItem(action_item, text, action);
        if (auto err = result) {
            return err;
        }

        m_list_actions->getList()->addItem(action_item);
    }

    return std::nullopt;
}

void RebaseViewWidget::updateActions() {
    std::vector<CommitAction> actions;
    auto* last      = m_new_commits_graph->addNode(0);
    auto& last_node = m_graph.first_node();
    m_root_node     = last_node.data;

    last->setCommit(last_node.commit.commit);
    last->setGitTree(last_node.data->getGitTree());

    m_last_new_commit = last;

    auto* list = m_list_actions->getList();
    for (int i = 0; i < list->count(); ++i) {
        auto* item = reinterpret_cast<ListItem*>(list->item(i));

        assert(item != nullptr);

        actions.push_back(item->getCommitAction());
    }

    prepareActions(actions);
}

Node* RebaseViewWidget::findOldCommit(std::string short_hash) {
    git_commit_t c;

    git_tree_t ancestor;
    if (!get_commit_from_hash(c, short_hash.c_str(), m_repo)) {
        return nullptr;
    }

    auto id = GitGraph<Node*>::get_commit_id(c.commit);
    if (!m_graph.contains(id)) {
        return nullptr;
    }

    auto& node = m_graph.get(id);

    return node.data;
}

void RebaseViewWidget::updateNode(Node* node, Node* current, Node* changes) {
    auto* current_commit = current->getCommit();
    auto* changes_commit = changes->getCommit();

    git_index_t index;

    if (git_cherrypick_commit(&index.index, m_repo, changes_commit, current_commit, 0, nullptr) != 0) {
        return;
    }

    if (git_index_has_conflicts(index.index) != 0) {
        node->setConflict(true);
    }
}

std::optional<std::string>
RebaseViewWidget::prepareItem(ListItem* item, QString& item_text, const CommitAction& action) {
    switch (action.type) {
    case CmdType::INVALID:
    case CmdType::NONE:
    case CmdType::EXEC:
    case CmdType::BREAK:
        break;
    case CmdType::DROP: {
        item->setItemColor(QColor(255, 62, 65));

        Node* old = findOldCommit(action.hash);
        assert(old != nullptr);
        item->addConnection(old);
        item->setNode(old);

        item_text += " [";
        item_text += action.hash.c_str();
        item_text += "]: ";
        item_text += git_commit_summary(old->getCommit());
        break;
    }

    case CmdType::FIXUP:
    case CmdType::SQUASH: {
        item->setItemColor(QColor(115, 137, 174));

        Node* old = findOldCommit(action.hash);
        assert(old != nullptr);
        item->addConnection(old);
        item->addConnection(m_last_new_commit);
        item->setNode(m_last_new_commit);

        updateNode(m_last_new_commit, m_last_new_commit, old);

        item_text += " [";
        item_text += action.hash.c_str();
        item_text += "]: ";
        item_text += git_commit_summary(old->getCommit());
        break;
    }

    case CmdType::RESET:
    case CmdType::LABEL:
    case CmdType::MERGE:
    case CmdType::UPDATE_REF:
        TODO("Implement");
        break;

    case CmdType::PICK:
    case CmdType::REWORD:
    case CmdType::EDIT: {
        Node* old = findOldCommit(action.hash);
        assert(old != nullptr);
        item->addConnection(old);

        item->setItemColor(QColor(49, 216, 67));
        item_text += " [";
        item_text += action.hash.c_str();
        item_text += "]: ";
        item_text += git_commit_summary(old->getCommit());

        Node* new_node = m_new_commits_graph->addNode();
        new_node->setCommit(old->getCommit());
        new_node->setGitTree(old->getGitTree());
        new_node->setParentNode(m_last_new_commit);

        item->addConnection(new_node);
        item->setNode(new_node);

        updateNode(new_node, m_last_new_commit, new_node);

        m_last_new_commit = new_node;
        break;
    }
    }

    item->setCommitAction(action);
    item->setText(item_text);
    return std::nullopt;
}
