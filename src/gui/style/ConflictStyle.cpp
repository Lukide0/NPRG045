#include "gui/style/ConflictStyle.h"
#include "gui/style/StyleManager.h"

#include <cassert>
#include <QColor>

namespace gui::style {

void ConflictStyle::set(Style style, const QColor& color) {
    assert(style >= 0 && style < Style::_LENGTH);
    m_colors[style] = color;
}

[[nodiscard]] const QColor& ConflictStyle::get(Style style) const {
    assert(style >= 0 && style < Style::_LENGTH);
    return m_colors[style];
}

void ConflictStyle::set_color(Style style, const QColor& color) {
    StyleManager::get_conflict_style().set(style, color);
}

const QColor& ConflictStyle::get_color(Style style) { return StyleManager::get_conflict_style().get(style); }

}
