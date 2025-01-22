#include "RebaseViewWidget.h"

#include "clear_layout.h"
#include "GitGraph.h"
#include "ListItem.h"
#include "NamedListWidget.h"
#include "parser.h"
#include "utils.h"
#include <cstdint>
#include <git2/types.h>
#include <optional>
#include <QListWidgetItem>
#include <QPalette>
#include <QSplitter>
#include <QWidget>
#include <span>
#include <string>
#include <utility>
#include <vector>

RebaseViewWidget::RebaseViewWidget(QWidget* parent)
    : QWidget(parent)
    , m_graph(GitGraph<Pos>::empty()) {
    m_layout = new QHBoxLayout();

    m_list_actions = new NamedListWidget("Actions");
    m_old_commits  = new QGridLayout();
    m_new_commits  = new QGridLayout();

    m_layout->addWidget(m_list_actions);
    m_layout->addLayout(m_old_commits);
    m_layout->addLayout(m_new_commits);

    setLayout(m_layout);

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
}

std::optional<std::string> RebaseViewWidget::update(
    git_repository* repo, const std::string& head, const std::string& onto, const std::vector<CommitAction>& actions
) {
    m_list_actions->clear();
    clear_layout(m_old_commits);
    clear_layout(m_new_commits);

    m_last_item = nullptr;

    auto graph_opt = GitGraph<Pos>::create(head.c_str(), onto.c_str(), repo);
    if (!graph_opt.has_value()) {
        return "Could not find commits";
    }

    m_graph                 = std::move(graph_opt.value());
    std::uint32_t max_depth = m_graph.max_depth();

    m_graph.reverse_iterate([&](std::uint32_t depth, std::span<GitNode<Pos>> nodes) {
        auto y = max_depth - depth;

        for (std::uint32_t x = 0; x < nodes.size(); ++x) {
            auto& node = nodes[x];

            auto text   = format_commit(node.commit.commit);
            auto* label = new QLabel(text.c_str());

            node.data = {
                .x = x,
                .y = y,
            };
            m_old_commits->addWidget(label, y, x);
        }
    });

    auto* last = new QLabel(format_commit(m_graph.first_node().commit.commit).c_str());
    m_new_commits->addWidget(last, 0, 0);
    m_last_pos = { .x = 0, .y = 0 };

    for (const auto& action : actions) {
        auto* action_item = new ListItem(m_list_actions->get_list());

        QString text = cmd_to_str(action.type);
        auto result  = prepare_item(action_item, text, action, repo);
        if (auto err = result) {
            return err;
        }

        m_list_actions->get_list()->addItem(action_item);
    }

    return std::nullopt;
}

QLabel* RebaseViewWidget::find_old_commit(std::string short_hash, git_repository* repo) {
    git_commit_t c;

    if (!get_commit_from_hash(c, short_hash.c_str(), repo)) {
        return nullptr;
    }

    auto id = GitGraph<Pos>::get_commit_id(c.commit);
    if (!m_graph.contains(id)) {
        return nullptr;
    }

    auto& node = m_graph.get(id);

    return get_label(node.data);
}

std::optional<std::string>
RebaseViewWidget::prepare_item(ListItem* item, QString& item_text, const CommitAction& action, git_repository* repo) {
    switch (action.type) {
    case CmdType::INVALID:
    case CmdType::NONE:
    case CmdType::EXEC:
    case CmdType::BREAK:
        break;
    case CmdType::DROP: {
        item->setItemColor(QColor(255, 62, 65));

        auto* old = find_old_commit(action.hash, repo);
        assert(old != nullptr);
        item->addConnection(old);

        item_text += " ";
        item_text += action.hash.c_str();
        break;
    }

    case CmdType::FIXUP:
    case CmdType::SQUASH: {
        item->setItemColor(QColor(115, 137, 174));

        auto* old = find_old_commit(action.hash, repo);
        assert(old != nullptr);
        item->addConnection(old);

        auto* new_label = get_new_label(m_last_pos);
        item->addConnection(new_label);

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

        auto* old = find_old_commit(action.hash, repo);
        assert(old != nullptr);
        item->addConnection(old);

        auto* new_label = new QLabel(old->text());
        m_new_commits->addWidget(new_label, m_last_pos.y + 1, m_last_pos.x);
        m_last_pos.y += 1;
        item->addConnection(new_label);
        break;
    }
    }

    item->setText(item_text);
    return std::nullopt;
}
