#pragma once

#include "action/Action.h"
#include "action/ActionManager.h"
#include "core/git/GitGraph.h"
#include "core/git/parser.h"
#include "core/git/types.h"
#include "gui/widget/CommitViewWidget.h"
#include "gui/widget/ConflictWidget.h"
#include "gui/widget/DiffWidget.h"
#include "gui/widget/graph/Graph.h"
#include "gui/widget/graph/Node.h"
#include "gui/widget/LineSplitter.h"
#include "gui/widget/ListItem.h"
#include "gui/widget/NamedListWidget.h"

#include <git2/oid.h>
#include <git2/types.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QObject>
#include <QPushButton>
#include <QSplitter>
#include <QStackedLayout>
#include <QWidget>

namespace gui::widget {

class RebaseViewWidget : public QWidget {
public:
    RebaseViewWidget(QWidget* parent = nullptr);
    std::optional<std::string> update(
        git_repository* repo,
        const std::string& head,
        const std::string& onto,
        const std::vector<core::git::CommitAction>& actions
    );

    std::optional<std::string> update(git_repository* repo, const std::string& head, const std::string& onto);

    void updateGraph();

    void updateActions();

    void hideOldCommits() { m_old_commits_graph->hide(); }

    void hideResultCommits() { m_new_commits_graph->hide(); }

    void showOldCommits() { m_old_commits_graph->show(); }

    void showResultCommits() { m_new_commits_graph->show(); }

    void moveAction(int from, int to);

    const auto& getActionsManager() const { return m_actions; }

    void ignoreMoveSignal(bool enable) { m_ignore_move = enable; }

private:
    /* UI */
    GraphWidget* m_old_commits_graph;
    GraphWidget* m_new_commits_graph;

    QListWidget* m_list_actions;

    CommitViewWidget* m_commit_view;
    DiffWidget* m_diff_widget;
    ConflictWidget* m_conflict_widget;

    QPushButton* m_resolve_conflicts_btn;
    QPushButton* m_mark_resolved_btn;

    int m_last_selected_index = -1;
    bool m_ignore_move        = false;

    Node* m_last_node = nullptr;

    core::git::GitGraph<Node*> m_graph;
    action::ActionsManager& m_actions;

    /* GIT */
    git_repository* m_repo;
    Node* m_root_node;
    core::git::tree_t m_conflict_parent_tree;
    core::git::index_t m_conflict_index;

    std::optional<std::string> prepareItem(ListItem* item, action::Action& action);

    std::optional<std::string> prepareActions();

    Node* findOldCommit(const git_oid& oid);

    void updateNode(ListItem* item, Node* node, Node* current, Node* changes);

    void showCommit(Node* prev, Node* next);

    void prepareGraph();

    ListItem* getListItem(int index) {
        auto* item = m_list_actions->item(index);
        if (item == nullptr) {
            return nullptr;
        }

        return dynamic_cast<ListItem*>(m_list_actions->itemWidget(item));
    }

    void changeItemSelection();

    void updateConflict(Node* node);

    bool updateConflictAction(action::Action* act);

    void checkoutAndResolve();

    void markResolved();
};
}
