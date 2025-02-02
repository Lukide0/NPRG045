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
#include <git2/commit.h>
#include <git2/types.h>
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
#include <sstream>
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

void RebaseViewWidget::showCommit(Node* /*unused*/, Node* next) {
    if (next == nullptr) {
        m_commit_view->hide();
        return;
    }

    auto* commit = next->getCommit();

    std::string hash           = GitGraph<Node*>::get_commit_id(commit);
    const char* summary        = git_commit_summary(commit);
    const char* desc           = git_commit_body(commit);
    const git_signature* autor = git_commit_author(commit);

    std::stringstream ss;
    {
        std::chrono::seconds seconds { autor->when.time };
        std::time_t time = seconds.count();
        std::tm* t       = std::gmtime(&time);

        ss << std::put_time(t, "%x %X");
    }
    auto time_str = ss.str();
    m_commit_view->update(hash.c_str(), summary, desc, autor->name, time_str.c_str());
    m_commit_view->show();
}

std::optional<std::string> RebaseViewWidget::update(
    git_repository* repo, const std::string& head, const std::string& onto, const std::vector<CommitAction>& actions
) {
    m_list_actions->clear();
    m_old_commits_graph->clear();
    m_new_commits_graph->clear();

    m_last_item = nullptr;

    auto graph_opt = GitGraph<Node*>::create(head.c_str(), onto.c_str(), repo);
    if (!graph_opt.has_value()) {
        return "Could not find commits";
    }

    m_graph = std::move(graph_opt.value());

    std::uint32_t max_depth = m_graph.max_depth();
    std::uint32_t max_width = m_graph.max_width();

    // TODO: Implement merge commits
    assert(max_width == 1);

    m_graph.reverse_iterate([&](std::uint32_t depth, std::span<GitNode<Node*>> nodes) {
        auto y = max_depth - depth;

        for (auto& node : nodes) {
            auto* commit_node = m_old_commits_graph->addNode(y);
            commit_node->setCommit(node.commit.commit);

            node.data = commit_node;
        }
    });

    auto* last = m_new_commits_graph->addNode(0);
    last->setCommit(m_graph.first_node().commit.commit);
    m_last_new_commit = last;

    for (const auto& action : actions) {
        auto* action_item = new ListItem(m_list_actions->get_list());

        QString text = cmd_to_str(action.type);
        auto result  = prepareItem(action_item, text, action, repo);
        if (auto err = result) {
            return err;
        }

        m_list_actions->get_list()->addItem(action_item);
    }

    return std::nullopt;
}

Node* RebaseViewWidget::findOldCommit(std::string short_hash, git_repository* repo) {
    git_commit_t c;

    if (!get_commit_from_hash(c, short_hash.c_str(), repo)) {
        return nullptr;
    }

    auto id = GitGraph<Node*>::get_commit_id(c.commit);
    if (!m_graph.contains(id)) {
        return nullptr;
    }

    auto& node = m_graph.get(id);

    return node.data;
}

std::optional<std::string> RebaseViewWidget::prepareItem(
    ListItem* item, QString& item_text, const CommitAction& action, [[maybe_unused]] git_repository* repo
) {
    switch (action.type) {
    case CmdType::INVALID:
    case CmdType::NONE:
    case CmdType::EXEC:
    case CmdType::BREAK:
        break;
    case CmdType::DROP: {
        item->setItemColor(QColor(255, 62, 65));

        auto* old = findOldCommit(action.hash, repo);
        assert(old != nullptr);
        item->addConnection(old);

        item_text += " ";
        item_text += action.hash.c_str();
        break;
    }

    case CmdType::FIXUP:
    case CmdType::SQUASH: {
        item->setItemColor(QColor(115, 137, 174));

        auto* old = findOldCommit(action.hash, repo);
        assert(old != nullptr);
        item->addConnection(old);
        item->addConnection(m_last_new_commit);

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

        auto* old = findOldCommit(action.hash, repo);
        assert(old != nullptr);
        item->addConnection(old);

        auto* new_node = m_new_commits_graph->addNode();
        new_node->setCommit(old->getCommit());
        item->addConnection(new_node);

        m_last_new_commit = new_node;
        break;
    }
    }

    item->setText(item_text);
    return std::nullopt;
}
