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

const QColor& DiffStyle::get_default_color(Style style) {
    assert(style >= 0 && style < Style::_LENGTH);
    return default_colors[style];
}

void DiffStyle::emit_changed() { emit changed(); }

void DiffStyle::emit_changed_signal() { StyleManager::get_diff_style().emit_changed(); }

void DiffStyle::save(QSettings& settings) const {
    settings.beginGroup("DiffStyle");
    {
        for (std::size_t i = 0; i < STYLE_NAMES.size(); ++i) {
            auto style       = static_cast<Style>(i);
            const char* name = STYLE_NAMES[i];

            settings.setValue(name, get_color(style));
        }
    }
    settings.endGroup();
}

void DiffStyle::load(QSettings& settings) {
    settings.beginGroup("DiffStyle");
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
