#pragma once

#include "core/action/Action.h"
#include "core/action/ActionsManager.h"

#include <functional>
#include <string>

#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <string_view>

namespace gui::widget {

class CommitMessageWidget : public QWidget {
    Q_OBJECT
public:
    using Action = core::action::Action;

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

    void clearText() { setEditorText(); }

    void openInEditor();
    void updateText();

    void setAction(const Action* action, std::string_view comment_str);

signals:
    void commitMessageChanged(const QString& msg);

private:
    QHBoxLayout* m_layout;
    QPlainTextEdit* m_editor;

    QPushButton* m_open_editor;
    QPushButton* m_update_msg;

    QString m_oid;
    QString m_filepath;

    void setEditorText(const QString& text) {
        m_editor->blockSignals(true);
        m_editor->setPlainText(text);
        m_editor->blockSignals(false);
    }

    void setEditorText() {
        m_editor->blockSignals(true);
        m_editor->clear();
        m_editor->blockSignals(false);
    }
};

}
