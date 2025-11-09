#include "gui/style/DiffStyle.h"
#include "gui/style/StyleManager.h"

#include <cassert>
#include <QColor>

namespace gui::style {

void DiffStyle::set(Style style, const QColor& color) {
    assert(style >= 0 && style < Style::_LENGTH);
    m_colors[style] = color;
}

[[nodiscard]] const QColor& DiffStyle::get(Style style) const {
    assert(style >= 0 && style < Style::_LENGTH);
    return m_colors[style];
}

void DiffStyle::set_color(Style style, const QColor& color) { StyleManager::get_diff_style().set(style, color); }

const QColor& DiffStyle::get_color(Style style) { return StyleManager::get_diff_style().get(style); }

}
