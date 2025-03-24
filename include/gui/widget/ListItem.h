#pragma once

#include "core/git/parser.h"
#include "core/state/Command.h"
#include "gui/widget/graph/Node.h"

#include <QBoxLayout>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QListWidgetItem>
#include <QPalette>
#include <QWidget>

#include <array>
#include <vector>

class RebaseViewWidget;

class ListItem : public QWidget {
public:
    static constexpr auto items = std::to_array<CmdType>({
        CmdType::PICK,
        CmdType::REWORD,
        CmdType::EDIT,
        CmdType::DROP,
        CmdType::FIXUP,
        CmdType::SQUASH,
    });

    static constexpr int indexOf(CmdType type) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (items[i] == type) {
                return i;
            }
        }

        return -1;
    }

    ListItem(RebaseViewWidget* rebase, QListWidget* list, int row);

    void setCommitAction(const CommitAction& action);

    QComboBox* getComboBox() { return m_combo; }

    [[nodiscard]] const CommitAction& getCommitAction() const { return m_action; }

    void setNode(Node* node) { m_node = node; }

    void setText(const QString& text) { m_text->setText(text); }

    Node* getNode() { return m_node; }

    void addConnection(Node* item) { m_connected.push_back(item); }

    void setItemColor(QColor color) { m_color = color; }

    [[nodiscard]] const QColor& getItemColor() const { return m_color; }

    static void handleChange(QListWidgetItem* curr) {
        if (curr != nullptr) {
            auto* item = dynamic_cast<ListItem*>(curr);
            if (item == nullptr) {
                return;
            }

            item->setColorToAll(item->m_color);
        }
    }

    [[nodiscard]] const auto& getConnected() const { return m_connected; }

    void setColorToAll(const QColor& color) {
        auto pal = palette();
        pal.setColor(backgroundRole(), color);
        setPalette(pal);

        for (auto* item : m_connected) {
            item->setFill(color);
        }
    }

    void setActionType(CmdType type) {
        m_action.type = type;

        auto index = indexOf(type);
        assert(index != -1);

        m_combo->setCurrentIndex(index);
    }

    void setActionTypeNoSignal(CmdType type) {
        m_combo->blockSignals(true);
        setActionType(type);
        m_combo->blockSignals(false);
    }

private:
    Node* m_node = nullptr;
    std::vector<Node*> m_connected;
    CommitAction m_action;
    QColor m_color;

    RebaseViewWidget* m_rebase;
    QListWidget* m_parent;
    int m_row;

    QLabel* m_text;
    QHBoxLayout* m_layout;

    QComboBox* m_combo;
};

class ListItemMoveCommand : public core::state::Command {
public:
    ListItemMoveCommand(RebaseViewWidget* rebase, QListWidget* parent, int prev_row, int curr_row);
    ~ListItemMoveCommand() override = default;

    void execute() override;
    void undo() override;

private:
    RebaseViewWidget* m_rebase;
    QListWidget* m_parent;
    int m_prev;
    int m_curr;

    void move(int from, int to);
};

class ListItemChangedCommand : public core::state::Command {
public:
    ListItemChangedCommand(RebaseViewWidget* rebase, QListWidget* parent, int row, CmdType prev, CmdType curr);
    ~ListItemChangedCommand() override = default;

    void execute() override;
    void undo() override;

private:
    RebaseViewWidget* m_rebase;
    QListWidget* m_parent;
    int m_row;
    CmdType m_prev;
    CmdType m_curr;

    void set_type(CmdType type);
};
