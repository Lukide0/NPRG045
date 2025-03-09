#pragma once

#include "gui/widget/DiffEditor.h"
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

class DiffFile : public QWidget {
public:
    DiffFile(QWidget* parent = nullptr)
        : QWidget(parent) {
        m_layout = new QVBoxLayout();
        setLayout(m_layout);

        m_label = new QLabel();

        QFont font = m_label->font();
        font.setBold(true);
        m_label->setFont(font);

        m_editor = new DiffEditor();

        m_layout->addWidget(m_label);
        m_layout->addWidget(m_editor, 1);
    }

    void setHeader(const QString& filepath) { m_label->setText(filepath); }

    DiffEditor* getEditor() { return m_editor; }

private:
    QVBoxLayout* m_layout;
    QLabel* m_label;
    DiffEditor* m_editor;
};
