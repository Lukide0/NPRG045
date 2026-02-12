#include "gui/widget/RebaseViewWidget.h"

#include "action/Action.h"
#include "action/ActionManager.h"
#include "core/conflict/conflict.h"
#include "core/conflict/conflict_iterator.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/commit.h"
#include "core/git/error.h"
#include "core/git/GitGraph.h"
#include "core/git/head.h"
#include "core/git/parser.h"
#include "core/git/types.h"
#include "core/state/CommandHistory.h"
#include "core/task/task.h"
#include "core/utils/debug.h"
#include "core/utils/todo.h"
#include "core/utils/unexpected.h"
#include "gui/color.h"
#include "gui/style/GlobalStyle.h"
#include "gui/style/StyleManager.h"
#include "gui/widget/CommitViewWidget.h"
#include "gui/widget/ConflictDialog.h"
#include "gui/widget/DiffWidget.h"
#include "gui/widget/graph/Graph.h"
#include "gui/widget/graph/Node.h"
#include "gui/widget/LineSplitter.h"
#include "gui/widget/ListItem.h"
#include "gui/widget/NamedListWidget.h"
#include "gui/widget/ScrollListWidget.h"
#include "logging/Log.h"

#include <cassert>
#include <cstdint>
#include <format>
#include <git2/refs.h>
#include <git2/repository.h>
#include <git2/signature.h>
#include <git2/tree.h>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <git2/checkout.h>
#include <git2/cherrypick.h>
#include <git2/commit.h>
#include <git2/errors.h>
#include <git2/index.h>
#include <git2/merge.h>
#include <git2/oid.h>
#include <git2/status.h>
#include <git2/types.h>

#include <QAbstractItemModel>
#include <QBoxLayout>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QList>
#include <QListWidgetItem>
#include <QMessageBox>
#include <qnamespace.h>
#include <QObject>
#include <QPalette>
#include <QPushButton>
#include <QSplitter>
#include <QWidget>

namespace gui::widget {

using action::Action;
using action::ActionType;

RebaseViewWidget::RebaseViewWidget(QWidget* parent)
    : QWidget(parent)
    , m_graph(core::git::GitGraph<Node*>::empty())
    , m_actions(action::ActionsManager::get())
    , m_conflict_manager(core::conflict::ConflictManager::get()) {
    using style::GlobalStyle;

    m_actions.clear();

    auto* widget_layout = new QHBoxLayout();
    widget_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(widget_layout);

    auto* horizontal_split    = new LineSplitter(Qt::Orientation::Horizontal);
    auto* left_split          = new LineSplitter(Qt::Orientation::Vertical);
    auto* diff_commit_split   = new LineSplitter(Qt::Orientation::Vertical);
    auto* diff_conflict_split = new LineSplitter(Qt::Orientation::Horizontal);

    auto* right_layout        = new QVBoxLayout();
    auto* right_layout_widget = new QWidget();
    right_layout_widget->setLayout(right_layout);

    horizontal_split->addWidget(left_split);
    horizontal_split->addWidget(right_layout_widget);

    widget_layout->addWidget(horizontal_split);

    auto* graphs_split = new LineSplitter(Qt::Orientation::Horizontal);

    //-- LEFT LAYOUT --------------------------------------------------------//
    m_list_actions = new ScrollListWidget();
    m_list_actions->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
    m_list_actions->setDragDropMode(QAbstractItemView::DragDropMode::InternalMove);
    m_list_actions->setDragEnabled(true);

    left_split->addWidget(m_list_actions);
    left_split->addWidget(graphs_split);

    m_old_commits_graph = new GraphWidget();
    m_new_commits_graph = new GraphWidget();

    graphs_split->addWidget(m_old_commits_graph);
    graphs_split->addWidget(m_new_commits_graph);

    //-- RIGHT LAYOUT -------------------------------------------------------//
    m_diff_widget     = new DiffWidget();
    m_commit_view     = new CommitViewWidget(m_diff_widget);
    m_conflict_widget = new ConflictWidget();

    m_conflict_widget->hide();

    // Buttons
    m_resolve_conflicts_btn = new QPushButton("Checkout and resolve conflicts");

    connect(m_resolve_conflicts_btn, &QPushButton::pressed, this, [this]() { checkoutAndResolve(); });

    auto* toolbar_layout = new QHBoxLayout();
    toolbar_layout->addWidget(m_resolve_conflicts_btn);

    m_resolve_conflicts_btn->setEnabled(false);

    right_layout->addLayout(toolbar_layout);
    right_layout->addWidget(diff_commit_split);

    diff_conflict_split->addWidget(m_diff_widget);
    diff_conflict_split->addWidget(m_conflict_widget);

    diff_commit_split->addWidget(diff_conflict_split);
    diff_commit_split->addWidget(m_commit_view);

    diff_commit_split->setStretchFactor(0, 2);
    diff_commit_split->setStretchFactor(1, 0);

    auto palette = m_list_actions->palette();
    palette.setColor(QPalette::Highlight, GlobalStyle::get_color(GlobalStyle::HIGHLIGHT));
    m_list_actions->setPalette(palette);

    connect(&style::StyleManager::get_global_style(), &GlobalStyle::changed, this, [this]() {
        auto p = m_list_actions->palette();
        auto c = GlobalStyle::get_color(GlobalStyle::HIGHLIGHT);
        LOG_INFO("Color: {}, {}, {}", c.red(), c.green(), c.blue());
        p.setColor(QPalette::Highlight, GlobalStyle::get_color(GlobalStyle::HIGHLIGHT));
        m_list_actions->setPalette(p);
    });

    connect(m_list_actions, &QListWidget::itemSelectionChanged, this, [this]() { changeItemSelection(); });

    auto handle = [&](Node* prev, Node* next) { this->showCommit(prev, next); };

    m_old_commits_graph->setHandle(handle);
    m_new_commits_graph->setHandle(handle);

    connect(
        m_list_actions->model(),
        &QAbstractItemModel::rowsMoved,
        this,
        [this](
            const QModelIndex& source, int source_row, int _end, const QModelIndex& _destination, int destination_row
        ) {
            assert(source_row == _end);
            assert(source == _destination);

            if (source_row == destination_row || m_ignore_move) {
                return;
            }

            if (source_row < destination_row) {
                destination_row -= 1;
            }

            core::state::CommandHistory::Add(
                std::make_unique<ListItemMoveCommand>(this, m_list_actions, source_row, destination_row)
            );

            this->moveAction(source_row, destination_row);
        }
    );
}

void RebaseViewWidget::changeItemSelection() {
    QListWidget* list = m_list_actions;
    auto selected     = list->selectedItems();
    if (selected.size() != 1) {
        m_commit_view->update(nullptr);
        showConflict(nullptr);
        return;
    }

    QListWidgetItem* item = selected.first();
    auto* list_item       = dynamic_cast<ListItem*>(list->itemWidget(item));
    if (list_item == nullptr) {
        return;
    }

    m_commit_view->update(list_item->getNode());
    showConflict(list_item->getNode());
}

void RebaseViewWidget::showConflict(Node* node) {
    using ConflictStatus = Action::ConflictStatus;

    m_resolve_conflicts_btn->setEnabled(false);

    if (node == nullptr || node->getAction() == nullptr) {
        m_conflict_widget->hide();
        return;
    }

    Action* act                    = node->getAction();
    ConflictStatus conflict_status = act->get_tree_status();

    switch (conflict_status) {
    case ConflictStatus::NO_CONFLICT:
    case ConflictStatus::UNKNOWN:
    case ConflictStatus::ERR:
        m_conflict_widget->hide();
        return;
    case ConflictStatus::RESOLVED_CONFLICT:
    case ConflictStatus::HAS_CONFLICT:
        break;
    }

    m_conflict_widget->show();
    m_resolve_conflicts_btn->setEnabled(true);
}

void RebaseViewWidget::updateConflictMarkers() {
    if (m_cherrypick == nullptr || m_cherrypick->get_prev() == nullptr) {
        return;
    }

    LOG_INFO("Updating conflict markers");

    // clear markers
    for (std::int32_t i = 0; i < m_list_actions->count(); ++i) {
        auto* item = getListItem(i);
        assert(item != nullptr);
        item->hideConflictMarker();
    }

    core::conflict::iterate_actions(
        *m_cherrypick,
        m_repo,
        m_conflict_files,
        [this](bool conflict, std::uint32_t action_id, void*) -> bool {
            if (conflict) {
                auto* item = getListItem(action_id);
                if (item != nullptr) {
                    item->showConflictMarker();
                }
            }
            return false;
        },
        nullptr
    );
}

void RebaseViewWidget::updateConflictList(Action* start) {
    using core::conflict::ConflictStatus;

    if (start == nullptr) {
        start = getActionsManager().get_first_action();
    } else {
        switch (start->get_tree_status()) {
        case ConflictStatus::UNKNOWN:
        case ConflictStatus::ERR:
            start = getActionsManager().get_first_action();
            break;

        case ConflictStatus::HAS_CONFLICT:
        case ConflictStatus::NO_CONFLICT:
        case ConflictStatus::RESOLVED_CONFLICT:
            break;
        }
    }

    for (Action* act = start; act != nullptr; act = act->get_next()) {
        updateConflictAction(act);
    }
}

Action::ConflictStatus RebaseViewWidget::updateConflictAction(Action* act) {
    using namespace core;
    using ConflictStatus = Action::ConflictStatus;

    if (act == nullptr) {
        return ConflictStatus::NO_CONFLICT;
    }
    // clear the resulting tree
    act->clear_tree();

    Action* parent_act = act->get_prev();

    ConflictStatus conflict_status;
    core::git::index_t conflict_index;

    // first action
    if (parent_act == nullptr) {
        git_commit* root_commit                   = getActionsManager().get_root_commit();
        std::tie(conflict_status, conflict_index) = conflict::cherrypick_check(act, root_commit);
    } else {
        std::tie(conflict_status, conflict_index) = conflict::cherrypick_check(act, parent_act);
    }

    switch (conflict_status) {
    // conflict::cherrypick_check does not return this value
    case ConflictStatus::RESOLVED_CONFLICT:
        UNEXPECTED();

    case ConflictStatus::ERR:
        utils::log_libgit_error();
        return ConflictStatus::UNKNOWN;

    case ConflictStatus::UNKNOWN:
        return ConflictStatus::UNKNOWN;

    case ConflictStatus::NO_CONFLICT: {
        // create tree from index
        git_oid oid;
        if (git_index_write_tree_to(&oid, conflict_index.get(), m_repo) != 0) {
            utils::log_libgit_error();
            return ConflictStatus::UNKNOWN;
        }
        core::git::tree_t tree;

        if (git_tree_lookup(&tree, m_repo, &oid) != 0) {
            utils::log_libgit_error();
            return ConflictStatus::UNKNOWN;
        }

        // update the action tree
        act->set_tree(std::move(tree), Action::ConflictStatus::NO_CONFLICT);
        return ConflictStatus::NO_CONFLICT;
    }
    case ConflictStatus::HAS_CONFLICT:
        break;
    }

    // update conflict
    m_conflict_index = std::move(conflict_index);
    m_cherrypick     = act;

    // prepare conflict widget
    m_conflict_widget->clearConflicts();
    m_conflict_paths.clear();
    m_conflict_entries.clear();
    m_conflict_files.clear();

    // clang-format off
    bool iterator_status = conflict::iterate(m_conflict_index.get(), [this](conflict::entry_data_t entry) -> bool {

        const char* path = nullptr;

        conflict::ConflictEntry conflict_entry;

        if (entry.our != nullptr) {
            conflict_entry.our_id = git_oid_tostr_s(&entry.our->id);
            path                  = entry.our->path;
        }

        if (entry.their != nullptr) {
            conflict_entry.their_id = git_oid_tostr_s(&entry.their->id);

            m_conflict_files.push_back(entry.their->id);

            if (path == nullptr) {
                path = entry.their->path;
            }
        }

        if (entry.ancestor != nullptr) {
            conflict_entry.ancestor_id = git_oid_tostr_s(&entry.ancestor->id);

            if (path == nullptr) {
                path = entry.ancestor->path;
            }
        }

        m_conflict_paths.emplace_back(path);
        m_conflict_entries.push_back(conflict_entry);

        if (!m_conflict_manager.is_resolved(conflict_entry)) {
            auto conflict_diff = core::git::create_conflict_diff(m_repo, entry.ancestor, entry.our, entry.their);

            LOG_INFO("Conflict in '{}'", path);

            if (!conflict_diff.has_value()) {
                utils::log_libgit_error();
                std::string diff = std::format("Failed to construct diff. Reason: {}", git::get_last_error());
                m_conflict_widget->addConflictFile(path, diff);
            } else {
                m_conflict_widget->addConflictFile(path, conflict_diff.value());
            }
        }

        return true;
    });
    // clang-format on

    if (!iterator_status) {
        utils::log_libgit_error();
        act->set_tree_status(Action::ConflictStatus::UNKNOWN);
        return ConflictStatus::UNKNOWN;
    }

    // check if all conflicts were resolved
    if (m_conflict_widget->hasConflict()) {
        act->set_tree_status(Action::ConflictStatus::HAS_CONFLICT);
        return ConflictStatus::HAS_CONFLICT;
    }

    if (!m_conflict_manager.apply_resolutions_no_write(
            m_conflict_entries, m_conflict_paths, m_repo, m_conflict_index.get()
        )) {
        utils::log_libgit_error();
        QMessageBox::critical(this, "Recorded resolution error", QString::fromStdString(git::get_last_error()));
        act->set_tree_status(Action::ConflictStatus::ERR);
        return ConflictStatus::UNKNOWN;
    }

    git_oid oid;
    if (git_index_write_tree_to(&oid, m_conflict_index, m_repo) != 0) {
        utils::log_libgit_error();
        QMessageBox::critical(this, "Recorded resolution error", QString::fromStdString(git::get_last_error()));
        act->set_tree_status(Action::ConflictStatus::ERR);
        return ConflictStatus::UNKNOWN;
    }

    git::tree_t tree;
    if (git_tree_lookup(&tree, m_repo, &oid) != 0) {
        utils::log_libgit_error();
        QMessageBox::critical(this, "Recorded resolution error", QString::fromStdString(git::get_last_error()));
        act->set_tree_status(Action::ConflictStatus::ERR);
        return ConflictStatus::UNKNOWN;
    }

    act->set_tree(std::move(tree), Action::ConflictStatus::RESOLVED_CONFLICT);
    return ConflictStatus::RESOLVED_CONFLICT;
}

void RebaseViewWidget::moveAction(int from, int to) {
    assert(from >= 0 && to >= 0 && from != to);

    LOG_INFO("Moving action: from {} to {}", from, to);

    Action* update_start = m_actions.move(from, to);
    updateConflictList(update_start);

    updateGraph();
    updateConflictMarkers();
}

void RebaseViewWidget::moveSelectedAction(bool down) {
    using core::state::CommandHistory;

    const int items_count = m_list_actions->count();
    int current           = m_list_actions->currentRow();

    if (current < 0) {
        if (items_count > 0) {
            m_list_actions->setCurrentRow(0);
        }
        return;
    }

    QAbstractItemModel* model = m_list_actions->model();

    int destination = (down) ? current + 2 : current - 1;

    // invalid destination
    if (destination < 0 || destination > items_count) {
        return;
    }

    if (model->moveRows(QModelIndex(), current, 1, QModelIndex(), destination)) {
        int row = (down) ? current + 1 : current - 1;
        m_list_actions->setCurrentRow(row);
    }
}

void RebaseViewWidget::changeActionType() {
    using core::state::CommandHistory;

    int current = m_list_actions->currentRow();
    if (current < 0) {
        return;
    }

    auto* item = getListItem(current);
    if (item == nullptr) {
        return;
    }

    ActionType type = item->getCommitAction().get_type();
    int index       = ListItem::indexOf(type);
    assert(index >= 0);

    int new_index       = (index + 1) % ListItem::items.size();
    ActionType new_type = ListItem::items[new_index];

    auto* cmd = new ListItemChangedCommand(m_list_actions, current, type, new_type);

    CommandHistory::Add(std::unique_ptr<ListItemChangedCommand>(cmd));

    cmd->execute();
}

void RebaseViewWidget::changeActionType(ActionType type) {
    using core::state::CommandHistory;

    int current = m_list_actions->currentRow();
    if (current < 0) {
        return;
    }

    auto* item = getListItem(current);
    if (item == nullptr) {
        return;
    }

    ActionType old_type = item->getCommitAction().get_type();
    // no change
    if (old_type == type) {
        return;
    }

    int index = ListItem::indexOf(old_type);
    assert(index >= 0);

    auto* cmd = new ListItemChangedCommand(m_list_actions, current, old_type, type);

    CommandHistory::Add(std::unique_ptr<ListItemChangedCommand>(cmd));

    cmd->execute();
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

    if (max_width > 1) {
        return "Merge commits are not supported";
    }

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
            return std::format(
                "Advanced rebase command not supported '{}'. Only basic commit actions (pick, edit, squash, etc.) are "
                "available.",
                core::git::cmd_to_str(action.type)
            );
        }
    }

    prepareActions();
    return std::nullopt;
}

std::optional<std::string>
RebaseViewWidget::update(git_repository* repo, const std::string& head, const std::string& onto) {

    m_old_commits_graph->clear();

    m_repo = repo;

    auto graph_opt = core::git::GitGraph<Node*>::create(head.c_str(), onto.c_str(), repo);
    if (!graph_opt.has_value()) {
        return "Could not find commits";
    }

    m_graph = std::move(graph_opt.value());

    std::uint32_t max_depth = m_graph.max_depth();
    std::uint32_t max_width = m_graph.max_width();

    if (max_width != 1) {
        return "Merge commits are not supported";
    }

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

    prepareActions();
    return std::nullopt;
}

void RebaseViewWidget::prepareActions() {

    prepareGraph();

    m_list_actions->clear();

    updateConflictList(nullptr);

    auto* list = m_list_actions;
    for (auto& action : m_actions) {
        auto* action_item = new ListItem(this, list, list->count(), action);

        prepareItem(action_item, action);

        auto* item = new QListWidgetItem();

        item->setSizeHint(action_item->sizeHint());
        list->addItem(item);
        list->setItemWidget(item, action_item);
    }

    updateConflictMarkers();
}

void RebaseViewWidget::prepareGraph() {
    m_new_commits_graph->clear();

    auto* last      = m_new_commits_graph->addNode(0);
    auto& last_node = m_graph.first_node();
    m_root_node     = last_node.data;

    last->setCommit(last_node.commit);
    last->setAction(m_actions.get_first_action());

    m_actions.set_root_commit(last_node.commit);

    m_last_node = last;

    m_commit_view->update(nullptr);
}

void RebaseViewWidget::updateActions() {
    LOG_INFO("Updating actions");

    prepareActions();

    m_commit_view->update(nullptr);
}

void RebaseViewWidget::updateGraph() {
    LOG_INFO("Updating graph");

    int last_selected_index = m_list_actions->currentRow();
    prepareGraph();

    auto* list  = m_list_actions;
    m_last_node = m_root_node;

    for (std::int32_t i = 0; i < list->count(); ++i) {
        auto* item = getListItem(i);
        assert(item != nullptr);

        Action& act = item->getCommitAction();
        item->setConflict(act.get_tree_status());

        switch (act.get_type()) {
        case ActionType::DROP:
            item->setNode(m_last_node);
            break;

        case ActionType::FIXUP:
        case ActionType::SQUASH:
            m_last_node->updateConflict(act.get_tree_status());

            item->setNode(m_last_node);
            break;

        case ActionType::PICK:
        case ActionType::REWORD:
        case ActionType::EDIT: {
            Node* new_node = m_new_commits_graph->addNode();
            new_node->setCommit(act.get_commit());
            new_node->setParentNode(m_last_node);
            new_node->setAction(&act);

            item->setNode(new_node);

            new_node->setConflict(act.get_tree_status());

            m_last_node = new_node;
            break;
        }
        }
    }

    if (last_selected_index == -1 || last_selected_index > m_list_actions->count()) {
        return;
    }

    auto* item = getListItem(last_selected_index);

    Node* node = nullptr;
    if (item != nullptr) {
        node = item->getNode();
    }

    showConflict(node);
    m_commit_view->update(node);
}

Node* RebaseViewWidget::findOldCommit(const git_oid& oid) {
    core::git::commit_t commit;

    if (git_commit_lookup(&commit, m_repo, &oid) != 0) {
        return nullptr;
    }

    return m_old_commits_graph->find([&](const Node* node) { return git_oid_equal(node->getCommitId(), &oid) != 0; });
}

void RebaseViewWidget::prepareItem(ListItem* item, Action& action) {
    QString item_text;
    item->setConflict(action.get_tree_status());

    switch (action.get_type()) {
    case ActionType::DROP: {
        Node* old = findOldCommit(action.get_oid());
        item->setNode(m_last_node);

        if (old != nullptr) {
            item_text += git_commit_summary(old->getCommit());
        } else {
            item_text += git_commit_summary(action.get_commit());
        }
        break;
    }

    case ActionType::FIXUP:
    case ActionType::SQUASH: {
        Node* old = findOldCommit(action.get_oid());
        if (old != nullptr) {
            m_last_node->updateConflict(action.get_tree_status());

            item_text += git_commit_summary(old->getCommit());
        } else {
            item_text += git_commit_summary(action.get_commit());
        }
        item->setNode(m_last_node);
        break;
    }

    case ActionType::PICK:
    case ActionType::REWORD:
    case ActionType::EDIT: {

        Node* old = findOldCommit(action.get_oid());
        if (old != nullptr) {
            item_text += git_commit_summary(old->getCommit());
        } else {
            item_text += git_commit_summary(action.get_commit());
        }

        Node* new_node = m_new_commits_graph->addNode();
        new_node->setCommit(action.get_commit());
        new_node->setParentNode(m_last_node);
        new_node->setAction(&action);

        item->setNode(new_node);
        new_node->setConflict(action.get_tree_status());

        m_last_node = new_node;
        break;
    }
    }

    item->setText(item_text);
}

void RebaseViewWidget::checkoutAndResolve() {
    using namespace core;

    // 1. Check if working index is clean
    {
        git_status_options opts = GIT_STATUS_OPTIONS_INIT;

        git_status_list* list;

        if (git_status_list_new(&list, m_repo, &opts) != 0) {
            utils::log_libgit_error();
            QMessageBox::critical(this, "Status error", QString::fromStdString(git::get_last_error()));
            return;
        }

        bool has_changes = (git_status_list_entrycount(list) != 0);
        if (has_changes) {
            LOG_ERROR("Working directory is not clean");
            QMessageBox::critical(this, "Status error", "Working directory is not clean");
            return;
        }
    }

    LOG_INFO("Working dir is clean");

    m_resolving.action        = nullptr;
    m_resolving.parent_action = nullptr;

    if (git_repository_head(&m_head, m_repo) != 0) {
        utils::log_libgit_error();
        QMessageBox::critical(this, "Repo head error", QString::fromStdString(git::get_last_error()));
        return;
    }

    LOG_INFO("Saving HEAD: '{}'", git_reference_name(m_head));

    // 2. Create temporary commit and checkout onto it
    {
        Action* parent_act      = m_cherrypick->get_prev();
        git_oid const* tree_oid = nullptr;
        if (parent_act == nullptr) {
            tree_oid = git_commit_tree_id(m_actions.get_root_commit());
        } else {
            tree_oid = git_tree_id(parent_act->get_tree());
        }

        assert(tree_oid != nullptr);

        git::tree_t tree;
        if (git_tree_lookup(&tree, m_repo, tree_oid) != 0) {
            utils::log_libgit_error();
            QMessageBox::critical(this, "Failed to load tree", QString::fromStdString(git::get_last_error()));
            return;
        }

        git::signature_t sig;
        if (git_signature_default(&sig, m_repo) != 0) {
            utils::log_libgit_error();
            QMessageBox::critical(this, "Failed to load signature", QString::fromStdString(git::get_last_error()));
            return;
        }

        git_oid commit_id;
        if (git_commit_create(&commit_id, m_repo, nullptr, sig, sig, nullptr, "Tmp commit", tree, 0, nullptr) != 0) {
            utils::log_libgit_error();
            QMessageBox::critical(
                this, "Failed to create temporary commit", QString::fromStdString(git::get_last_error())
            );
            return;
        }

        if (!git::set_repository_head_detached(m_repo, &commit_id)) {
            utils::log_libgit_error();
            QMessageBox::critical(this, "Repo head error", QString::fromStdString(git::get_last_error()));
            return;
        }

        LOG_INFO("Checking out tree");
    }

    // 3. Checkout conflict
    {
        git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

        opts.checkout_strategy |= GIT_CHECKOUT_SAFE | GIT_CHECKOUT_ALLOW_CONFLICTS | GIT_CHECKOUT_RECREATE_MISSING;

        if (git_checkout_index(m_repo, m_conflict_index.get(), &opts) != 0) {
            utils::log_libgit_error();
            QMessageBox::critical(this, "Repo head error", QString::fromStdString(git::get_last_error()));
            return;
        }

        LOG_INFO("Checking out index with conflicts");

        m_resolve_conflicts_btn->setEnabled(false);
    }

    // 4. Apply conflict resolutions
    {
        git::index_t index;
        if (git_repository_index(&index, m_repo) != 0) {
            utils::log_libgit_error();
            QMessageBox::critical(this, "Repo error", QString::fromStdString(git::get_last_error()));
            return;
        }

        if (!m_conflict_manager.apply_resolutions(m_conflict_entries, m_conflict_paths, m_repo, index.get())) {
            utils::log_libgit_error();
            QMessageBox::critical(this, "Recorded resolution error", QString::fromStdString(git::get_last_error()));
            return;
        }
    }

    m_resolving.action        = m_cherrypick;
    m_resolving.parent_action = m_cherrypick->get_prev();

    // 5. Create dialog to prevent user to modifing application state
    {
        ConflictDialog dialog(this);

        dialog.exec();

        while (dialog.resolved()) {

            // check if the resolution can be applied
            if (markResolved()) {
                break;
            }

            dialog.exec();
        }

        // clean working directory
        if (!dialog.resolved()) {
            LOG_INFO("Restoring HEAD: '{}'", git_reference_name(m_head));

            if (!git::set_repository_head(m_repo, m_head.get())) {
                utils::log_libgit_error();
                QMessageBox::critical(
                    this, "Failed to clean working directory", QString::fromStdString(git::get_last_error())
                );
            }

            m_resolve_conflicts_btn->setEnabled(true);
        }
    }
}

bool RebaseViewWidget::markResolved() {
    using namespace core;

    if (m_conflict_index.get() == nullptr || m_resolving.action == nullptr) {
        return false;
    }

    git::index_t repo_index;
    if (git_repository_index(&repo_index, m_repo) != 0) {
        utils::log_libgit_error();
        QMessageBox::critical(this, "Repo index", QString::fromStdString(git::get_last_error()));
        return false;
    }

    // 1. Add all modified files to the index and create new tree
    auto&& [err, tree_oid]
        = conflict::add_resolved_files(repo_index, m_repo, m_conflict_paths, m_conflict_entries, m_conflict_manager);
    if (err.has_value()) {
        LOG_ERROR("{}", err.value());
        QMessageBox::critical(this, "Resolution error", QString::fromStdString(err.value()));
        return false;
    }

    git::tree_t tree;
    if (git_tree_lookup(&tree, m_repo, &tree_oid) != 0) {
        utils::log_libgit_error();
        QMessageBox::critical(this, "Tree error", QString::fromStdString(git::get_last_error()));
        return false;
    }

    LOG_INFO("Restoring HEAD: '{}'", git_reference_name(m_head));
    // Change working tree
    if (!git::set_repository_head(m_repo, m_head)) {
        utils::log_libgit_error();
        QMessageBox::critical(this, "Repo head error", QString::fromStdString(git::get_last_error()));
        return false;
    }

    conflict::ConflictTrees conflict;

    git_oid const* tree_id;
    if (m_cherrypick->get_prev() != nullptr) {
        tree_id = git_tree_id(m_cherrypick->get_prev()->get_tree());
    } else {
        git_commit* root_commit = m_actions.get_root_commit();
        tree_id                 = git_commit_tree_id(root_commit);
    }

    auto& conflict_manager  = conflict::ConflictManager::get();
    conflict.commit_id      = git::format_oid_to_str<git::OID_SIZE>(&m_cherrypick->get_oid());
    conflict.parent_tree_id = git::format_oid_to_str<git::OID_SIZE>(tree_id);

    conflict_manager.add_trees_resolution(conflict, std::move(tree));

    LOG_INFO("Saving conflict resolution");

    m_resolving.action        = nullptr;
    m_resolving.parent_action = nullptr;

    updateActions();

    return true;
}

}
