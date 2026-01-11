#pragma once

#include <QPlainTextEdit>

namespace gui {

inline void update_editor_height(QPlainTextEdit* editor) {
    auto* document    = editor->document();
    auto lines        = document->lineCount() + 1;
    auto line_spacing = editor->fontMetrics().lineSpacing();
    auto margins      = editor->contentsMargins();
    auto height       = (lines * line_spacing) + margins.top() + margins.bottom();

    editor->setFixedHeight(height);
}

}
