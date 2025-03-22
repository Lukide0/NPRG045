#include "gui/widget/DiffEditorLine.h"
#include "core/git/diff.h"
#include "core/utils/unexpected.h"

QColor DiffEditorLine::ConvertToColor(Type type) {
    switch (type) {
    case Type::ADDITION:
        return { 6, 214, 160 };
    case Type::DELETION:
        return { 239, 91, 111 };
    case Type::HUNK_INFO:
        return { 44, 7, 156 };
    }

    UNEXPECTED("Invalid type");
}

QColor DiffEditorLine::ConvertToColor(diff_line_t::Type type) {
    switch (type) {
    case diff_line_t::Type::CONTEXT:
    case diff_line_t::Type::CONTEXT_NO_NEWLINE:
        return {};
    case diff_line_t::Type::ADDITION:
    case diff_line_t::Type::ADDITION_NEWLINE:
        return { 6, 214, 160 };
    case diff_line_t::Type::DELETION:
    case diff_line_t::Type::DELETION_NEWLINE:
        return { 239, 91, 111 };
    }

    UNEXPECTED("Invalid type");
}

char DiffEditorLine::ConvertToSymbol(diff_line_t::Type type) {
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

    UNEXPECTED("Invalid type");
}
