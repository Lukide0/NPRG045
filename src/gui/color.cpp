#include "gui/color.h"
#include "core/git/diff.h"
#include "core/utils/unexpected.h"

QColor convert_to_color(ColorType type) {
    switch (type) {
    case ColorType::NORMAL:
        return {};
    case ColorType::ADDITION:
        return { 6, 214, 160 };
    case ColorType::DELETION:
        return { 239, 91, 111 };
    case ColorType::INFO:
        return { 44, 7, 156 };
    }

    UNEXPECTED("Invalid color type");
}

QColor convert_to_color(diff_line_t::Type type) {
    switch (type) {
    case diff_line_t::Type::CONTEXT:
    case diff_line_t::Type::CONTEXT_NO_NEWLINE:
        return convert_to_color(ColorType::NORMAL);
    case diff_line_t::Type::ADDITION:
    case diff_line_t::Type::ADDITION_NEWLINE:
        return convert_to_color(ColorType::ADDITION);
    case diff_line_t::Type::DELETION:
    case diff_line_t::Type::DELETION_NEWLINE:
        return convert_to_color(ColorType::DELETION);
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
