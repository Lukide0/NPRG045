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

const QColor& ConflictStyle::get_default_color(Style style) {
    assert(style >= 0 && style < Style::_LENGTH);
    return default_colors[style];
}

void ConflictStyle::emit_changed() { emit changed(); }

void ConflictStyle::emit_changed_signal() { StyleManager::get_conflict_style().emit_changed(); }

void ConflictStyle::save(QSettings& settings) const {
    settings.beginGroup("ConflictStyle");
    {
        for (std::size_t i = 0; i < STYLE_NAMES.size(); ++i) {
            auto style       = static_cast<Style>(i);
            const char* name = STYLE_NAMES[i];

            settings.setValue(name, get_color(style));
        }
    }
    settings.endGroup();
}

void ConflictStyle::load(QSettings& settings) {
    settings.beginGroup("ConflictStyle");
    {
        for (std::size_t i = 0; i < STYLE_NAMES.size(); ++i) {
            auto style       = static_cast<Style>(i);
            const char* name = STYLE_NAMES[i];

            auto color = settings.value(name, default_colors[i]).value<QColor>();

            set_color(style, color);
        }
    }
    settings.endGroup();
}

}
