#pragma once

#include "action/Action.h"
#include "action/ActionManager.h"
#include "gui/file.h"
#include <functional>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QWidget>
#include <string>

namespace gui::widget {

class CommitMessageWidget : public QWidget {
public:
    using Action = action::Action;

    CommitMessageWidget(QWidget* parent = nullptr);

    void enableEdit() {
        m_editor->setReadOnly(false);
        m_open_editor->setEnabled(true);
        m_update_msg->setEnabled(true);
    }

    void disableEdit() {
        m_editor->setReadOnly(true);
        m_open_editor->setEnabled(false);
        m_update_msg->setEnabled(false);
    }

    void setText(const QString& text) {
        setEditorText(text);

        m_filepath.clear();
    }

    void openInEditor();
    void updateText();

    void setAction(Action* action);

    Action* getAction() { return m_action; }

    void setTextChangeHandle(std::function<void(const std::string&)> handle) { m_handle = handle; }

private:
    QHBoxLayout* m_layout;
    QPlainTextEdit* m_editor;

    QPushButton* m_open_editor;
    QPushButton* m_update_msg;

    Action* m_action = nullptr;
    QString m_filepath;
    action::ActionsManager& m_manager;
    std::function<void(const std::string&)> m_handle = nullptr;

    void setEditorText(const QString& text) {
        m_editor->blockSignals(true);
        m_editor->setPlainText(text);
        m_editor->blockSignals(false);
    }
};

}
