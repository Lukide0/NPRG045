#pragma once

#include "commit/CommitViewWidget.h"
#include "GitGraph.h"
#include "graph/Graph.h"
#include "graph/Node.h"
#include "ListItem.h"
#include "NamedListWidget.h"
#include "parser.h"

#include <git2/types.h>
#include <optional>
#include <qboxlayout.h>
#include <qgridlayout.h>
#include <qlabel.h>
#include <QListWidgetItem>
#include <qobject.h>
#include <QWidget>
#include <string>
#include <vector>

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

    ListItem* m_last_item = nullptr;

    std::optional<std::string>
    prepareItem(ListItem* item, QString& item_text, const CommitAction& action, git_repository* repo);

    Node* findOldCommit(std::string short_hash, git_repository* repo);

    void showCommit(Node* prev, Node* next);
};
