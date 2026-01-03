#include "gui/color.h"
#include "core/git/diff.h"
#include "core/utils/unexpected.h"
#include "gui/style/DiffStyle.h"
#include "gui/style/StyleManager.h"

#include <QColor>

namespace gui {

using core::git::diff_line_t;

style::DiffStyle::Style convert_to_diff_color(diff_line_t::Type type) {
    switch (type) {
    case diff_line_t::Type::CONTEXT:
    case diff_line_t::Type::CONTEXT_NO_NEWLINE:
        return style::DiffStyle::NORMAL;
    case diff_line_t::Type::ADDITION:
    case diff_line_t::Type::ADDITION_NEWLINE:
        return style::DiffStyle::ADDITION;
    case diff_line_t::Type::DELETION:
    case diff_line_t::Type::DELETION_NEWLINE:
        return style::DiffStyle::DELETION;
    }

    UNEXPECTED("Invalid diff line type");
}

char convert_to_symbol(diff_line_t::Type type) {
    switch (type) {
    case diff_line_t::Type::CONTEXT:
    case diff_line_t::Type::CONTEXT_NO_NEWLINE:
        return ' ';
    case diff_line_t::Type::ADDITION:
    case diff_line_t::Type::ADDITION_NEWLINE:
        return '+';
    case diff_line_t::Type::DELETION:
    case diff_line_t::Type::DELETION_NEWLINE:
        return '-';
    }

    UNEXPECTED("Invalid diff line type");
}

QColor get_highlight_color() { return { 84, 106, 123 }; }

}
