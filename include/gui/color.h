#pragma once

#include "core/git/diff.h"
#include "gui/style/DiffStyle.h"

#include <QColor>

namespace gui {

enum class ColorType {
    NORMAL,
    ADDITION,
    DELETION,
    INFO,
};

QColor get_highlight_color();
style::DiffStyle::Style convert_to_diff_color(core::git::diff_line_t::Type type);

char convert_to_symbol(core::git::diff_line_t::Type type);

}
