#pragma once

#include "gui/widget/graph/Node.h"
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QWidget>

class CommitMessageWidget : public QWidget {
public:
    CommitMessageWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        m_layout = new QHBoxLayout();
        setLayout(m_layout);

        m_editor = new QPlainTextEdit();
        m_editor->setReadOnly(true);

        m_layout->addWidget(m_editor);
    }

    void clear() { m_editor->clear(); }

    void setMsg(QString msg) { m_editor->setPlainText(msg); }

    void enableEdit() { m_editor->setReadOnly(false); }

    void disableEdit() { m_editor->setReadOnly(true); }

    void setMsg(Node* node);

private:
    QHBoxLayout* m_layout;
    QPlainTextEdit* m_editor;
};

inline void CommitMessageWidget::setMsg(Node* node) {
    assert(node != nullptr);
    auto* commit = node->getCommit();

    const char* msg = git_commit_message(commit);
    m_editor->setPlainText(msg);
}
