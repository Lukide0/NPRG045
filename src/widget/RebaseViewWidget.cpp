#include "widget/RebaseViewWidget.h"

#include "git/GitGraph.h"
#include "git/parser.h"
#include "git/types.h"
#include "widget/CommitViewWidget.h"
#include "widget/graph/Graph.h"
#include "widget/graph/Node.h"
#include "widget/ListItem.h"
#include "widget/NamedListWidget.h"

#include <cassert>
#include <cstdint>
#include <ctime>
#include <git2/cherrypick.h>
#include <git2/commit.h>
#include <git2/index.h>
#include <git2/merge.h>
#include <git2/types.h>
#include <iostream>
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

RebaseViewWidget::RebaseViewWidget(QWidget* parent)
    : QWidget(parent)
    , m_graph(GitGraph<Node*>::empty()) {
    m_main_layout = new QVBoxLayout();
    m_layout      = new QHBoxLayout();

    m_list_actions      = new NamedListWidget("Actions");
    m_old_commits_graph = new GraphWidget(this);
    m_new_commits_graph = new GraphWidget(this);

    m_layout->addWidget(m_list_actions);
    m_layout->addWidget(m_old_commits_graph);
    m_layout->addWidget(m_new_commits_graph);

    m_commit_view = new CommitViewWidget();
    m_commit_view->hide();

    m_main_layout->addItem(m_layout);
    m_main_layout->addWidget(m_commit_view);

    setLayout(m_main_layout);

    connect(m_list_actions->get_list(), &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (m_last_item != nullptr) {
            m_last_item->setColorToAll(Qt::white);
        }

        if (item != nullptr) {

            auto* list_item = dynamic_cast<ListItem*>(item);
            if (list_item == nullptr) {
                return;
            }

            list_item->setColorToAll(list_item->getItemColor());
            m_last_item = list_item;
        }
    });

    auto handle = [&](Node* prev, Node* next) { this->showCommit(prev, next); };

    m_old_commits_graph->setHandle(handle);
    m_new_commits_graph->setHandle(handle);
}

void RebaseViewWidget::showCommit(Node* prev, Node* next) {
    if (next == nullptr) {
        m_commit_view->hide();
        return;
    }

    if (prev == next) {
        m_commit_view->show();
        return;
    }

    m_commit_view->update(next);
    m_commit_view->show();
}

std::optional<std::string> RebaseViewWidget::update(
    git_repository* repo, const std::string& head, const std::string& onto, const std::vector<CommitAction>& actions
) {
    m_list_actions->clear();
    m_old_commits_graph->clear();
    m_new_commits_graph->clear();

    m_last_item = nullptr;
    m_repo      = repo;

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

            commit_node->setGitTree(tree);

            node.data = commit_node;
            node.data->setParentNode(parent);
            parent = commit_node;
        }
    });

    if (!err_msg.empty()) {
        return err_msg;
    }

    auto* last      = m_new_commits_graph->addNode(0);
    auto& last_node = m_graph.first_node();
    m_root_node     = last_node.data;

    last->setCommit(last_node.commit.commit);
    last->setGitTree(last_node.data->getGitTree());

    m_last_new_commit = last;

    for (const auto& action : actions) {
        auto* action_item = new ListItem(m_list_actions->get_list());

        QString text = cmd_to_str(action.type);
        auto result  = prepareItem(action_item, text, action);
        if (auto err = result) {
            return err;
        }

        m_list_actions->get_list()->addItem(action_item);
    }

    return std::nullopt;
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

        item_text += " ";
        item_text += action.hash.c_str();
        break;
    }

    case CmdType::FIXUP:
    case CmdType::SQUASH: {
        item->setItemColor(QColor(115, 137, 174));

        Node* old = findOldCommit(action.hash);
        assert(old != nullptr);
        item->addConnection(old);
        item->addConnection(m_last_new_commit);

        updateNode(m_last_new_commit, m_last_new_commit, old);

        item_text += " ";
        item_text += action.hash.c_str();
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

        item->setItemColor(QColor(49, 216, 67));
        item_text += " ";
        item_text += action.hash.c_str();

        Node* old = findOldCommit(action.hash);
        assert(old != nullptr);
        item->addConnection(old);

        Node* new_node = m_new_commits_graph->addNode();
        new_node->setCommit(old->getCommit());
        new_node->setGitTree(old->getGitTree());
        new_node->setParentNode(m_last_new_commit);

        item->addConnection(new_node);

        updateNode(new_node, m_last_new_commit, new_node);

        m_last_new_commit = new_node;
        break;
    }
    }

    item->setText(item_text);
    return std::nullopt;
}
