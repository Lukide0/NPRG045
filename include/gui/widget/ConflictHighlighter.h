#pragma once

#include "core/utils/strings.h"
#include "gui/style/ConflictStyle.h"
#include "gui/style/StyleManager.h"
#include "logging/Log.h"

#include <git2/merge.h>
#include <QSyntaxHighlighter>
#include <QWidget>

namespace gui::widget {

class ConflictHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    ConflictHighlighter(QTextDocument* parent)
        : QSyntaxHighlighter(parent) {

        using style::ConflictStyle;
        using Style = style::ConflictStyle::Style;

        m_style.setForeground(ConflictStyle::get_color(Style::CONFLICT));
        connect(&style::StyleManager::get_conflict_style(), &style::ConflictStyle::changed, this, [this]() {
            m_style.setForeground(ConflictStyle::get_color(Style::CONFLICT));
            rehighlight();
        });
    }

private:
    QTextCharFormat m_style;

    static constexpr int NORMAL_BLOCK   = 0;
    static constexpr int CONFLICT_BLOCK = 1;

    static constexpr std::size_t MARKER_SIZE = GIT_MERGE_CONFLICT_MARKER_SIZE;
    using marker_t                           = core::utils::comptime_str_array<MARKER_SIZE + 1>;

    static constexpr auto START_BLOCK_MARKER = marker_t('<');
    static constexpr auto END_BLOCK_MARKER   = marker_t('>');

    void highlightBlock(const QString& text) override {

        if (text.startsWith(START_BLOCK_MARKER.c_str())) {
            setFormat(0, text.length(), m_style);
            setCurrentBlockState(CONFLICT_BLOCK);
            return;
        }

        if (previousBlockState() == CONFLICT_BLOCK) {
            setFormat(0, text.length(), m_style);

            if (text.startsWith(END_BLOCK_MARKER.c_str())) {
                setCurrentBlockState(NORMAL_BLOCK);
            } else {
                setCurrentBlockState(CONFLICT_BLOCK);
            }
            return;
        }

        setCurrentBlockState(NORMAL_BLOCK);
    }
};

}
