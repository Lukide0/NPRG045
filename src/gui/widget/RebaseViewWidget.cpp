#include "gui/widget/RebaseViewWidget.h"

#include "action/Action.h"
#include "action/ActionManager.h"
#include "core/conflict/conflict.h"
#include "core/conflict/conflict_iterator.h"
#include "core/git/commit.h"
#include "core/git/error.h"
#include "core/git/GitGraph.h"
#include "core/git/head.h"
#include "core/git/parser.h"
#include "core/git/types.h"
#include "core/state/CommandHistory.h"
#include "core/utils/debug.h"
#include "core/utils/todo.h"
#include "gui/color.h"
#include "gui/widget/CommitViewWidget.h"
#include "gui/widget/DiffWidget.h"
#include "gui/widget/graph/Graph.h"
#include "gui/widget/graph/Node.h"
#include "gui/widget/LineSplitter.h"
#include "gui/widget/ListItem.h"
#include "gui/widget/NamedListWidget.h"
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
    m_list_actions = new QListWidget();
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
    m_mark_resolved_btn     = new QPushButton("Mark as resolved");

    connect(m_resolve_conflicts_btn, &QPushButton::pressed, this, [this]() { checkoutAndResolve(); });
    connect(m_mark_resolved_btn, &QPushButton::pressed, this, [this]() { markResolved(); });

    auto* toolbar_layout = new QHBoxLayout();
    toolbar_layout->addWidget(m_resolve_conflicts_btn);
    toolbar_layout->addWidget(m_mark_resolved_btn);

    m_mark_resolved_btn->setEnabled(false);
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
    palette.setColor(QPalette::Highlight, get_highlight_color());

    m_list_actions->setPalette(palette);

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

            m_last_selected_index = destination_row;

            this->updateGraph();
        }
    );
}

void RebaseViewWidget::changeItemSelection() {
    QListWidget* list = m_list_actions;
    auto selected     = list->selectedItems();
    if (selected.size() != 1) {
        m_last_selected_index = -1;
        m_commit_view->update(nullptr);
        updateConflict(nullptr);
        return;
    }

    QListWidgetItem* item = selected.first();
    auto* list_item       = dynamic_cast<ListItem*>(list->itemWidget(item));
    if (list_item == nullptr) {
        return;
    }

    m_last_selected_index = list_item->getRow();

    m_commit_view->update(list_item->getNode());

    updateConflict(list_item->getNode());
}

void RebaseViewWidget::updateConflict(Node* node) {

    m_mark_resolved_btn->setEnabled(false);
    m_resolve_conflicts_btn->setEnabled(false);

    if (node == nullptr) {
        m_conflict_widget->hide();
        return;
    }

    Action* act                    = node->getAction();
    const bool in_progress         = (act == m_resolving.action && act->get_prev() == m_resolving.parent_action);
    ConflictStatus conflict_status = updateConflictAction(act);

    switch (conflict_status) {
    case ConflictStatus::NO_CONFLICT:
        m_conflict_widget->hide();
        return;
    case ConflictStatus::RESOLVED:
    case ConflictStatus::NOT_RESOLVED:
        break;
    }

    m_conflict_widget->show();

    if (in_progress) {
        // check if the HEAD is correct
        auto* commit           = action::ActionsManager::get_picked_parent_commit(m_cherrypick);
        const auto* commit_oid = git_commit_id(commit);

        if (core::git::is_head(m_repo, commit_oid)) {
            m_mark_resolved_btn->setEnabled(true);
            return;
        }
    }

    m_resolve_conflicts_btn->setEnabled(true);
}

RebaseViewWidget::ConflictStatus RebaseViewWidget::updateConflictAction(Action* act) {
    using namespace core;

    if (act == nullptr) {
        return ConflictStatus::NO_CONFLICT;
    }

    switch (act->get_type()) {
    case action::ActionType::DROP:
        return ConflictStatus::NO_CONFLICT;
    case action::ActionType::SQUASH:
    case action::ActionType::FIXUP:
    case action::ActionType::PICK:
    case action::ActionType::REWORD:
    case action::ActionType::EDIT:
        break;
    }

    auto* commit        = act->get_commit();
    auto* parent_commit = action::ActionsManager::get_picked_parent_commit(act);

    auto&& [status, conflict] = conflict::cherrypick_check(commit, parent_commit);
    switch (status) {
    case conflict::ConflictStatus::ERR:
        utils::log_libgit_error();
        return ConflictStatus::NO_CONFLICT;
    case conflict::ConflictStatus::NO_CONFLICT:
        return ConflictStatus::NO_CONFLICT;
    case conflict::ConflictStatus::HAS_CONFLICT:
        break;
    }

    m_conflict_index = std::move(conflict);

    m_conflict_parent_tree.destroy();
    if (git_commit_tree(&m_conflict_parent_tree, parent_commit) != 0) {
        m_conflict_parent_tree = nullptr;
    }

    m_conflict_widget->clearConflicts();
    m_conflict_paths.clear();
    m_conflict_entries.clear();

    m_cherrypick = act;

    bool iterator_status = conflict::iterate(m_conflict_index.get(), [this](conflict::entry_data_t entry) -> bool {
        const char* path = entry.their->path;

        conflict::ConflictEntry conflict_entry;
        if (entry.ancestor != nullptr) {
            conflict_entry.ancestor_id = git_oid_tostr_s(&entry.ancestor->id);
        }

        if (entry.their != nullptr) {
            conflict_entry.their_id = git_oid_tostr_s(&entry.their->id);
        }

        if (entry.our != nullptr) {
            conflict_entry.our_id = git_oid_tostr_s(&entry.our->id);
        }

        if (path == nullptr) {
            path = entry.our->path;
        }

        if (path == nullptr) {
            path = entry.ancestor->path;
        }

        m_conflict_paths.emplace_back(path);
        m_conflict_entries.push_back(conflict_entry);

        if (!m_conflict_manager.is_resolved(conflict_entry)) {
            m_conflict_widget->addConflictFile(path);
        }

        return true;
    });

    if (!iterator_status) {
        utils::log_libgit_error();
    }

    if (m_conflict_widget->isEmpty()) {
        return ConflictStatus::RESOLVED;
    } else {
        return ConflictStatus::NOT_RESOLVED;
    }
}

void RebaseViewWidget::moveAction(int from, int to) {
    assert(from >= 0 && to >= 0 && from != to);

    LOG_INFO("Moving action: from {} to {}", from, to);

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

    m_last_selected_index = -1;
    m_list_actions->clear();

    auto* list = m_list_actions;
    for (auto& action : m_actions) {
        auto* action_item = new ListItem(this, list, list->count(), action);

        prepareItem(action_item, action);

        auto* item = new QListWidgetItem();

        item->setSizeHint(action_item->sizeHint());
        list->addItem(item);
        list->setItemWidget(item, action_item);
    }
}

void RebaseViewWidget::prepareGraph() {
    m_new_commits_graph->clear();

    m_last_selected_index = -1;

    auto* last      = m_new_commits_graph->addNode(0);
    auto& last_node = m_graph.first_node();
    m_root_node     = last_node.data;

    last->setCommit(last_node.commit);
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
    LOG_INFO("Updating old graph");

    const int last_selected_index = m_last_selected_index;

    prepareGraph();

    auto* list = m_list_actions;

    m_last_node = m_root_node;

    for (std::int32_t i = 0; i < list->count(); ++i) {
        auto* raw_item = list->itemWidget(list->item(i));
        auto* item     = dynamic_cast<ListItem*>(raw_item);
        assert(item != nullptr);

        item->setConflict(false);

        Action& act = item->getCommitAction();

        switch (act.get_type()) {
        case ActionType::DROP: {
            item->setNode(m_last_node);
            break;
        }
        case ActionType::FIXUP:
        case ActionType::SQUASH: {
            Node* old = findOldCommit(act.get_oid());
            if (old != nullptr) {
                updateNode(item, m_last_node, m_last_node, old);
            }

            item->setNode(m_last_node);
            break;
        }

        case ActionType::PICK:
        case ActionType::REWORD:
        case ActionType::EDIT: {
            Node* new_node = m_new_commits_graph->addNode();
            new_node->setCommit(act.get_commit());
            new_node->setParentNode(m_last_node);
            new_node->setAction(&act);

            item->setNode(new_node);

            updateNode(item, new_node, m_last_node, new_node);

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

        if (node != nullptr) {
            m_last_selected_index = last_selected_index;
        }
    }

    updateConflict(node);
    m_commit_view->update(node);
}

Node* RebaseViewWidget::findOldCommit(const git_oid& oid) {
    core::git::commit_t commit;

    if (git_commit_lookup(&commit, m_repo, &oid) != 0) {
        return nullptr;
    }

    return m_old_commits_graph->find([&](const Node* node) { return git_oid_equal(node->getCommitId(), &oid) != 0; });
}

void RebaseViewWidget::updateNode(ListItem* item, Node* node, Node* parent, Node* current) {
    using core::conflict::ConflictStatus;
    using core::git::index_t;

    auto* parent_commit = parent->getCommit();
    auto* commit        = current->getCommit();

    auto&& [status, conflict] = core::conflict::cherrypick_check(commit, parent_commit);

    item->setConflict(false);

    bool not_resolved = !m_conflict_manager.is_resolved(conflict);

    switch (status) {
    case ConflictStatus::ERR:
        core::utils::log_libgit_error();
        return;
    case ConflictStatus::NO_CONFLICT:
        return;
    case ConflictStatus::HAS_CONFLICT:
        item->setConflict(not_resolved);
        node->setConflict(not_resolved);
        break;
    }
}

void RebaseViewWidget::prepareItem(ListItem* item, Action& action) {
    QString item_text;
    item_text += " [";
    item_text += QString::fromStdString(core::git::format_oid_to_str<7>(&action.get_oid()));
    item_text += "]: ";

    item->setConflict(false);

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
            updateNode(item, m_last_node, m_last_node, old);
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

        updateNode(item, new_node, m_last_node, new_node);

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

    // save HEAD
    if (git_repository_head(&m_head, m_repo) != 0) {
        utils::log_libgit_error();
        QMessageBox::critical(this, "Repo head error", QString::fromStdString(git::get_last_error()));
        return;
    }

    // 2. Checkout first commit
    {
        auto* commit = action::ActionsManager::get_picked_parent_commit(m_cherrypick);

        if (!git::set_repository_head_detached(m_repo, git_commit_id(commit))) {
            utils::log_libgit_error();
            QMessageBox::critical(this, "Repo head error", QString::fromStdString(git::get_last_error()));
            return;
        }

        LOG_INFO("Checking out commit");
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
        m_mark_resolved_btn->setEnabled(true);
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
}

void RebaseViewWidget::markResolved() {
    using namespace core;

    if (m_conflict_index.get() == nullptr) {
        return;
    }

    git::index_t repo_index;
    if (git_repository_index(&repo_index, m_repo) != 0) {
        utils::log_libgit_error();
        QMessageBox::critical(this, "Repo index", QString::fromStdString(git::get_last_error()));
        return;
    }

    // 1. Add all modified files to the index and create new tree
    auto&& [err, tree_oid]
        = conflict::add_resolved_files(repo_index, m_repo, m_conflict_paths, m_conflict_entries, m_conflict_manager);
    if (err.has_value()) {
        LOG_ERROR("{}", err.value());
        QMessageBox::critical(this, "Resolution error", QString::fromStdString(err.value()));
        return;
    }

    git::tree_t tree;
    if (git_tree_lookup(&tree, m_repo, &tree_oid) != 0) {
        utils::log_libgit_error();
        QMessageBox::critical(this, "Tree error", QString::fromStdString(git::get_last_error()));
        return;
    }

    // Change working tree
    if (!git::set_repository_head(m_repo, m_head.get())) {
        utils::log_libgit_error();
        QMessageBox::critical(this, "Repo head error", QString::fromStdString(git::get_last_error()));
        return;
    }

    m_mark_resolved_btn->setEnabled(false);

    auto* parent_commit = action::ActionsManager::get_picked_parent_commit(m_cherrypick);

    conflict::ConflictCommits conflict;

    auto& conflict_manager = conflict::ConflictManager::get();
    conflict.child_id      = git::format_oid_to_str<git::OID_SIZE>(&m_cherrypick->get_oid());
    conflict.parent_id     = git::format_oid_to_str<git::OID_SIZE>(git_commit_id(parent_commit));

    conflict_manager.add_commits_resolution(conflict, std::move(tree));

    LOG_INFO("Saving conflict resolution");

    m_resolving.action        = nullptr;
    m_resolving.parent_action = nullptr;

    updateActions();
}
}
