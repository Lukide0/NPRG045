#pragma once

#include "git/GitGraph.h"
#include "git/parser.h"
#include "widget/CommitViewWidget.h"
#include "widget/graph/Graph.h"
#include "widget/graph/Node.h"
#include "widget/ListItem.h"
#include "widget/NamedListWidget.h"

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

    void hideOldCommits() { m_old_commits_graph->hide(); }

    void showOldCommits() { m_old_commits_graph->show(); }

private:
    QHBoxLayout* m_layout;
    QVBoxLayout* m_main_layout;

    CommitViewWidget* m_commit_view;
    NamedListWidget* m_list_actions;

    GraphWidget* m_old_commits_graph;
    GraphWidget* m_new_commits_graph;
    Node* m_last_new_commit = nullptr;

    GitGraph<Node*> m_graph;
    git_repository* m_repo;

    ListItem* m_last_item = nullptr;
    Node* m_root_node;

private:
    std::optional<std::string> prepareItem(ListItem* item, QString& item_text, const CommitAction& action);

    std::optional<std::string> prepareActions(const std::vector<CommitAction>& actions);

    Node* findOldCommit(std::string short_hash);

    void updateNode(Node* node, Node* current, Node* changes);

    void updateActions();

    void showCommit(Node* prev, Node* next);
};
