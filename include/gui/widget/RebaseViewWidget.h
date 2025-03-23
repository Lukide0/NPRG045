#pragma once

#include "core/git/GitGraph.h"
#include "core/git/parser.h"
#include "gui/widget/CommitViewWidget.h"
#include "gui/widget/DiffWidget.h"
#include "gui/widget/graph/Graph.h"
#include "gui/widget/graph/Node.h"
#include "gui/widget/ListItem.h"
#include "gui/widget/NamedListWidget.h"

#include <git2/types.h>
#include <optional>
#include <qboxlayout.h>
#include <qgridlayout.h>
#include <qlabel.h>
#include <QListWidgetItem>
#include <qobject.h>
#include <QWidget>
#include <string>

class RebaseViewWidget : public QWidget {
public:
    RebaseViewWidget(QWidget* parent = nullptr);
    std::optional<std::string> update(
        git_repository* repo, const std::string& head, const std::string& onto, const std::vector<CommitAction>& actions
    );
    void updateActions();

    void hideOldCommits() { m_old_commits_graph->hide(); }

    void showOldCommits() { m_old_commits_graph->show(); }

private:
    QHBoxLayout* m_layout;
    // Contains: actions and graphs
    QVBoxLayout* m_left_layout;
    // Contains: diff and commit
    QVBoxLayout* m_right_layout;

    QHBoxLayout* m_graphs_layout;

    GraphWidget* m_old_commits_graph;
    GraphWidget* m_new_commits_graph;

    NamedListWidget* m_list_actions;

    CommitViewWidget* m_commit_view;
    DiffWidget* m_diff_widget;

    Node* m_last_new_commit = nullptr;

    GitGraph<Node*> m_graph;
    git_repository* m_repo;

    ListItem* m_last_item = nullptr;
    Node* m_root_node;

private:
    std::optional<std::string> prepareItem(ListItem* item, const CommitAction& action);

    std::optional<std::string> prepareActions(const std::vector<CommitAction>& actions);

    Node* findOldCommit(std::string short_hash);

    void updateNode(Node* node, Node* current, Node* changes);

    void showCommit(Node* prev, Node* next);
};
