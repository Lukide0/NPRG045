#pragma once

#include "GitGraph.h"
#include "ListItem.h"
#include "NamedListWidget.h"
#include "parser.h"
#include <cstdint>
#include <git2/types.h>
#include <optional>
#include <QListWidgetItem>
#include <QWidget>
#include <string>
#include <vector>

class RebaseViewWidget : public QWidget {
public:
    RebaseViewWidget(QWidget* parent = nullptr);
    std::optional<std::string> update(
        git_repository* repo, const std::string& head, const std::string& onto, const std::vector<CommitAction>& actions
    );

private:
    struct Pos {
        std::uint32_t x;
        std::uint32_t y;
    };

    QHBoxLayout* m_layout;
    NamedListWidget* m_list_actions;
    QGridLayout* m_old_commits;
    QGridLayout* m_new_commits;
    GitGraph<Pos> m_graph;

    ListItem* m_last_item = nullptr;
    Pos m_last_pos;

    std::optional<std::string>
    prepare_item(ListItem* item, QString& item_text, const CommitAction& action, git_repository* repo);

    QLabel* get_label(Pos pos) { return dynamic_cast<QLabel*>(m_old_commits->itemAtPosition(pos.y, pos.x)->widget()); }

    QLabel* get_new_label(Pos pos) {
        return dynamic_cast<QLabel*>(m_new_commits->itemAtPosition(pos.y, pos.x)->widget());
    }

    QLabel* find_old_commit(std::string short_hash, git_repository* repo);
};
