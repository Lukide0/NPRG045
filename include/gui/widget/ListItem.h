#pragma once

#include "action/Action.h"
#include "core/state/Command.h"
#include "gui/widget/graph/Node.h"
#include "logging/Log.h"

#include <QBoxLayout>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QListWidgetItem>
#include <QPalette>
#include <QString>
#include <QWidget>

#include <array>
#include <cassert>
#include <vector>

namespace gui::widget {

class RebaseViewWidget;

class ListItem : public QWidget {
public:
    using ActionType = action::ActionType;

    static constexpr auto items = std::to_array<ActionType>({
        ActionType::PICK,
        ActionType::DROP,
        ActionType::SQUASH,
        ActionType::FIXUP,
        ActionType::REWORD,
        ActionType::EDIT,
    });

    static constexpr int indexOf(ActionType type) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (items[i] == type) {
                return i;
            }
        }

        return -1;
    }

    ListItem(RebaseViewWidget* rebase, QListWidget* list, int row, action::Action& action);

    QComboBox* getComboBox() { return m_combo; }

    [[nodiscard]] const action::Action& getCommitAction() const { return m_action; }

    [[nodiscard]] action::Action& getCommitAction() { return m_action; }

    void setNode(Node* node) { m_node = node; }

    void setText(const QString& text) { m_text->setText(text); }

    Node* getNode() { return m_node; }

    void addConnection(Node* item) { m_connected.push_back(item); }

    void clearConnections() { m_connected.clear(); }

    void setItemColor(QColor color) {
        auto p = palette();
        p.setColor(QPalette::Text, color);
        setPalette(p);

        m_color = color;
    }

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

    [[nodiscard]] int getRow() const { return m_row; }

    [[nodiscard]] const auto& getConnected() const { return m_connected; }

    void setColorToAll(const QColor& color) {
        auto pal = palette();
        pal.setColor(backgroundRole(), color);
        setPalette(pal);

        for (auto* item : m_connected) {
            item->setFill(color);
        }
    }

    void setActionType(ActionType type) {
        LOG_INFO(
            "Changing action type: from {} to {}",
            action::Action::type_to_str(m_action.get_type()),
            action::Action::type_to_str(type)
        );

        m_action.set_type(type);

        auto index = indexOf(type);
        assert(index != -1);

        m_combo->setCurrentIndex(index);
    }

    void setActionTypeNoSignal(ActionType type) {
        m_combo->blockSignals(true);
        setActionType(type);
        m_combo->blockSignals(false);
    }

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    Node* m_node = nullptr;
    std::vector<Node*> m_connected;
    action::Action& m_action;
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
    ListItemChangedCommand(QListWidget* parent, int row, action::ActionType prev, action::ActionType curr);
    ~ListItemChangedCommand() override = default;

    void execute() override;
    void undo() override;

private:
    QListWidget* m_parent;
    int m_row;
    action::ActionType m_prev;
    action::ActionType m_curr;

    void set_type(action::ActionType type);
};
}
