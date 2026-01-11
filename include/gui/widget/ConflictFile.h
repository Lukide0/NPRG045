#pragma once

#include "gui/editor_height.h"
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::widget {

class ConflictFile : public QWidget {
public:
    ConflictFile(const QString& path, QWidget* parent = nullptr)
        : QWidget(parent) {
        m_layout = new QVBoxLayout();
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->addStretch();
        setLayout(m_layout);

        m_label = new QLabel(path);

        QFont font = m_label->font();
        font.setBold(true);
        m_label->setFont(font);

        m_editor = new QPlainTextEdit();

        m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_editor->setFrameShape(QFrame::NoFrame);
        m_editor->setReadOnly(true);

        m_layout->addWidget(m_label);
        m_layout->addWidget(m_editor);
    }

    void setContent(const QString& content) { m_editor->setPlainText(content); }

    void updateEditorHeight() { update_editor_height(m_editor); }

private:
    QVBoxLayout* m_layout;
    QLabel* m_label;
    QPlainTextEdit* m_editor;
};

}
