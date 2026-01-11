#pragma once

#include "core/git/diff.h"
#include "gui/editor_height.h"
#include "gui/widget/DiffEditor.h"
#include <QLabel>
#include <QString>
#include <qtmetamacros.h>
#include <QVBoxLayout>
#include <QWidget>
#include <string>
#include <utility>

namespace gui::widget {

class DiffFile : public QWidget {
    Q_OBJECT
public:
    DiffFile(const core::git::diff_files_t& diff, QWidget* parent = nullptr)
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

        m_editor         = new DiffEditor(diff);
        auto line_offset = m_editor->diffLineWidth();

        m_label->setContentsMargins(line_offset, 0, 0, 0);

        m_layout->addWidget(m_editor);
        m_layout->addWidget(m_label);

        connect(m_editor, &DiffEditor::lineOrFileSelect, this, [this](bool selected) {
            QString baseText = m_label->text();
            QPalette palette = m_label->palette();

            // remove prefix
            if (baseText.startsWith("● ") || baseText.startsWith("○ ")) {
                baseText = baseText.mid(2);
            }

            if (selected) {
                m_label->setText("● " + baseText);
                palette.setColor(QPalette::WindowText, QColor(0, 120, 215));
            } else {
                m_label->setText("○ " + baseText);
                palette.setColor(QPalette::WindowText, palette.color(QPalette::Text));
            }

            m_label->setPalette(palette);
        });
    }

    void setHeader(const QString& filepath) { m_label->setText("○ " + filepath); }

    void setDiff(core::git::diff_files_header_t diff) { m_diff = std::move(diff); }

    [[nodiscard]] const core::git::diff_files_header_t& getDiff() const { return m_diff; }

    DiffEditor* getEditor() { return m_editor; }

    void updateEditorHeight() { update_editor_height(m_editor); }

private:
    QVBoxLayout* m_layout;
    QLabel* m_label;
    DiffEditor* m_editor;
    core::git::diff_files_header_t m_diff;
};

}
