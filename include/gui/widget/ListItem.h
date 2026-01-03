#pragma once

#include "action/Action.h"
#include "core/conflict/conflict.h"
#include "core/state/Command.h"
#include "gui/color.h"
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

    static constexpr auto items = action::action_types;
    using ConflictStatus        = core::conflict::ConflictStatus;

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

    [[nodiscard]] int getRow() const { return m_row; }

    void setConflict(ConflictStatus status);

    void setActionType(ActionType type) {
        LOG_INFO(
            "Changing action type: from {} to {}", action::type_to_str(m_action.get_type()), action::type_to_str(type)
        );
        auto index = indexOf(type);
        assert(index != -1);

        m_combo->setCurrentIndex(index);
        m_action.set_type(type);
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
    action::Action& m_action;
    core::conflict::ConflictStatus m_conflict;

    QColor m_original_highlight;

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
