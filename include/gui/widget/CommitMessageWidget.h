#pragma once

#include "action/Action.h"
#include "action/ActionManager.h"
#include "gui/widget/graph/Node.h"
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QWidget>

class CommitMessageWidget : public QWidget {
public:
    CommitMessageWidget(ActionsManager& manager, QWidget* parent = nullptr);

    void clear() {
        m_action = nullptr;
        m_editor->clear();
    }

    void enableEdit() { m_editor->setReadOnly(false); }

    void disableEdit() { m_editor->setReadOnly(true); }

    void setText(QString text) {
        m_editor->blockSignals(true);
        m_editor->setPlainText(text);
        m_editor->blockSignals(false);
    }

    void setAction(Action* action);

    Action* getAction() { return m_action; }

private:
    QHBoxLayout* m_layout;
    QPlainTextEdit* m_editor;
    Action* m_action = nullptr;
    ActionsManager& m_manager;
};
