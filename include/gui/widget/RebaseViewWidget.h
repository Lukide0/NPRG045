#pragma once

#include "action/Action.h"
#include "action/ActionManager.h"
#include "core/git/GitGraph.h"
#include "core/git/parser.h"
#include "gui/widget/CommitViewWidget.h"
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
#include <QSplitter>
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
    void updateActions();

    void hideOldCommits() { m_old_commits_graph->hide(); }

    void hideResultCommits() { m_new_commits_graph->hide(); }

    void showOldCommits() { m_old_commits_graph->show(); }

    void showResultCommits() { m_new_commits_graph->show(); }

    void moveAction(int from, int to);

    const auto& getActionsManager() const { return m_actions; }

private:
    QHBoxLayout* m_layout;
    // Contains: actions and graphs
    LineSplitter* m_left_split;
    // Contains: diff, commit and commit message
    LineSplitter* m_right_split;
    LineSplitter* m_diff_commit_split;

    LineSplitter* m_horizontal_split;

    LineSplitter* m_graphs_split;

    GraphWidget* m_old_commits_graph;
    GraphWidget* m_new_commits_graph;

    NamedListWidget* m_list_actions;

    CommitViewWidget* m_commit_view;
    DiffWidget* m_diff_widget;

    Node* m_last_new_commit = nullptr;

    core::git::GitGraph<Node*> m_graph;
    action::ActionsManager& m_actions;
    git_repository* m_repo;

    ListItem* m_last_item = nullptr;
    Node* m_root_node;

    std::optional<std::string> prepareItem(ListItem* item, action::Action& action);

    std::optional<std::string> prepareActions();

    Node* findOldCommit(const git_oid& oid);

    void updateNode(Node* node, Node* current, Node* changes);

    void showCommit(Node* prev, Node* next);
};

}
