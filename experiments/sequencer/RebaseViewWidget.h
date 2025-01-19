#pragma once

#include "clear_layout.h"
#include "ListItem.h"
#include "NamedListWidget.h"
#include "parser.h"
#include "utils.h"
#include <git2/types.h>
#include <optional>
#include <QListWidgetItem>
#include <QWidget>
#include <string>
#include <unordered_map>
#include <vector>

class RebaseViewWidget : public QWidget {
public:
    RebaseViewWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        m_layout = new QHBoxLayout(this);

        m_list_actions     = new NamedListWidget("Actions", this);
        m_list_old_commits = new NamedListWidget("Old Commits", this);
        m_list_new_commits = new NamedListWidget("New Commits", this);

        m_layout->addWidget(m_list_actions);
        m_layout->addWidget(m_list_old_commits);
        m_layout->addWidget(m_list_new_commits);

        m_list_old_commits->get_list()->setSelectionMode(QAbstractItemView::NoSelection);
        m_list_new_commits->get_list()->setSelectionMode(QAbstractItemView::NoSelection);

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

    std::optional<std::string> update(
        git_repository* repo, const std::string& head, const std::string& onto, const std::vector<CommitAction>& actions
    ) {
        m_list_actions->clear();
        m_list_new_commits->clear();
        m_list_old_commits->clear();

        m_last_item = nullptr;

        std::vector<git_commit_t> commits;
        if (!get_all_commits_in_range(commits, head.c_str(), onto.c_str(), repo)) {
            return "Could not find commits";
        }

        std::unordered_map<std::string, ListItem*> actions_map;

        QListWidgetItem* last_commit;
        {
            auto* first_commit = new QListWidgetItem(m_list_new_commits->get_list());
            auto commit_text   = format_commit(commits.front().commit);

            first_commit->setText(commit_text.c_str());
            m_list_new_commits->get_list()->addItem(first_commit);

            last_commit = first_commit;
        }

        for (const auto& action : actions) {
            auto* action_item = new ListItem(m_list_actions->get_list());

            QString text = cmd_to_str(action.type);

            std::string key  = action.hash.substr(0, 7);
            actions_map[key] = action_item;

            auto result = prepare_item(action_item, text, action, last_commit, repo);
            if (auto err = result) {
                return err;
            }

            m_list_actions->get_list()->addItem(action_item);
        }

        for (auto&& commit : commits) {
            auto* item = new QListWidgetItem(m_list_old_commits->get_list());

            auto item_text = format_commit(commit.commit);
            item->setText(item_text.c_str());

            // format commit hash to 7 chars
            auto id         = format_oid(commit.commit);
            std::string key = id.data();

            if (!actions_map.contains(key)) {
                continue;
            }

            auto* action = actions_map[key];
            action->addConnection(item);
        }

        return std::nullopt;
    }

private:
    QHBoxLayout* m_layout;
    NamedListWidget* m_list_actions;
    NamedListWidget* m_list_new_commits;
    NamedListWidget* m_list_old_commits;

    ListItem* m_last_item = nullptr;

    std::optional<std::string> prepare_item(
        ListItem* item,
        QString& item_text,
        const CommitAction& action,
        QListWidgetItem*& last_commit,
        git_repository* repo
    ) {
        switch (action.type) {
        case CmdType::INVALID:
        case CmdType::NONE:
        case CmdType::EXEC:
        case CmdType::BREAK:
            break;
        case CmdType::DROP:
            item->setItemColor(QColor(255, 62, 65));
            item_text += " ";
            item_text += action.hash.c_str();
            break;

        case CmdType::FIXUP:
        case CmdType::SQUASH:
            item->setItemColor(QColor(115, 137, 174));
            item->addConnection(last_commit);
            item_text += " ";
            item_text += action.hash.c_str();
            break;

        case CmdType::RESET:
        case CmdType::LABEL:
        case CmdType::MERGE:
        case CmdType::UPDATE_REF:
            TODO("Implement");
            break;

        case CmdType::PICK:
        case CmdType::REWORD:
        case CmdType::EDIT:
            item->setItemColor(QColor(49, 216, 67));

            if (auto* item_commit = create_commit_item(action.hash.c_str(), repo)) {
                m_list_new_commits->get_list()->addItem(item_commit);
                last_commit = item_commit;

                item->addConnection(item_commit);
            } else {
                return std::string("Commit not found: ") + action.hash;
            }

            item_text += " ";
            item_text += action.hash.c_str();
            break;
        }

        item->setText(item_text);
        return std::nullopt;
    }

    static QListWidgetItem* create_commit_item(const char* hash, git_repository* repo) {

        git_commit_t commit;
        if (!get_commit_from_hash(commit, hash, repo)) {
            return nullptr;
        }

        auto text = format_commit(commit.commit);
        return new QListWidgetItem(text.c_str());
    }
};
