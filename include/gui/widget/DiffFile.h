#pragma once

#include "git/diff.h"
#include "gui/editor_height.h"
#include "gui/style/GlobalStyle.h"
#include "gui/style/StyleManager.h"
#include "gui/widget/DiffEditor.h"

#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QString>
#include <Qt>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

namespace gui::widget {

class DiffFile : public QWidget {
    Q_OBJECT
public:
    static constexpr const char* NOT_SELECTED_PREFIX = "\uf1db ";
    static constexpr const char* SELECTED_PREFIX     = "\uf192 ";

    DiffFile(const git::diff_files_t& diff, QWidget* parent = nullptr)
        : QWidget(parent) {
        using style::GlobalStyle;

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
            if (baseText.startsWith(SELECTED_PREFIX) || baseText.startsWith(NOT_SELECTED_PREFIX)) {
                baseText = baseText.mid(2);
            }

            m_selected = selected;

            if (selected) {
                m_label->setText(SELECTED_PREFIX + baseText);
                palette.setColor(QPalette::WindowText, GlobalStyle::get_color(GlobalStyle::Style::HIGHLIGHT));
            } else {
                m_label->setText(NOT_SELECTED_PREFIX + baseText);
                palette.setColor(QPalette::WindowText, palette.color(QPalette::Text));
            }

            m_label->setPalette(palette);
        });

        connect(&style::StyleManager::get_global_style(), &style::GlobalStyle::changed, this, [this]() {
            if (!m_selected) {
                return;
            }

            auto p = m_label->palette();
            p.setColor(QPalette::WindowText, GlobalStyle::get_color(GlobalStyle::HIGHLIGHT));
            m_label->setPalette(p);
        });
    }

    void setHeader(const QString& filepath) { m_label->setText(NOT_SELECTED_PREFIX + filepath); }

    void setDiff(git::diff_files_header_t diff) { m_diff = std::move(diff); }

    [[nodiscard]] const git::diff_files_header_t& getDiff() const { return m_diff; }

    DiffEditor* getEditor() { return m_editor; }

    void updateEditorHeight() { update_editor_height(m_editor); }

private:
    QVBoxLayout* m_layout;
    QLabel* m_label;
    DiffEditor* m_editor;
    git::diff_files_header_t m_diff;
    bool m_selected = false;
};

}
