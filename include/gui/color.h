#pragma once

#include "core/git/diff.h"
#include <QColor>

namespace gui {

enum class ColorType {
    NORMAL,
    ADDITION,
    DELETION,
    INFO,
};

QColor get_highlight_color();

QColor convert_to_color(ColorType type);
QColor convert_to_color(core::git::diff_line_t::Type type);
char convert_to_symbol(core::git::diff_line_t::Type type);

}
