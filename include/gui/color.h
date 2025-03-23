#pragma once

#include "core/git/diff.h"
#include <QColor>

enum class ColorType {
    NORMAL,
    ADDITION,
    DELETION,
    INFO,
};

QColor convert_to_color(ColorType type);
QColor convert_to_color(diff_line_t::Type type);
char convert_to_symbol(diff_line_t::Type type);
