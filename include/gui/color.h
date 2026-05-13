#pragma once

#include "git/diff.h"
#include "gui/style/DiffStyle.h"

#include <QColor>

namespace gui {

enum class ColorType {
    NORMAL,
    ADDITION,
    DELETION,
    INFO,
};

style::DiffStyle::Style convert_to_diff_color(git::diff_line_t::Type type);

char convert_to_symbol(git::diff_line_t::Type type);

}
