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
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->addStretch();
        m_layout->setDirection(QBoxLayout::BottomToTop);
        setLayout(m_layout);

        m_label = new QLabel();

        QFont font = m_label->font();
        font.setBold(true);
        m_label->setFont(font);

        m_editor         = new DiffEditor();
        auto line_offset = m_editor->diffLineWidth();

        m_label->setContentsMargins(line_offset, 0, 0, 0);

        m_layout->addWidget(m_editor);
        m_layout->addWidget(m_label);
    }

    void setHeader(const QString& filepath) { m_label->setText(filepath); }

    DiffEditor* getEditor() { return m_editor; }

    void updateEditorHeight() {
        auto* document    = m_editor->document();
        auto lines        = document->lineCount() + 1;
        auto line_spacing = m_editor->fontMetrics().lineSpacing();
        auto margins      = m_editor->contentsMargins();
        auto height       = (lines * line_spacing) + margins.top() + margins.bottom();

        m_editor->setFixedHeight(height);
    }

private:
    QVBoxLayout* m_layout;
    QLabel* m_label;
    DiffEditor* m_editor;
};
