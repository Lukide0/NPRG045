#include "gui/style/ConflictStyle.h"

#include "gui/style/StyleManager.h"

#include <cassert>
#include <cstddef>

#include <QColor>
#include <QSettings>

namespace gui::style {

void ConflictStyle::set(Style style, const QColor& color) {
    assert(style >= 0 && style < Style::_LENGTH);
    m_colors[style] = color;
}

[[nodiscard]] const QColor& ConflictStyle::get(Style style) const {
    assert(style >= 0 && style < Style::_LENGTH);
    return m_colors[style];
}

void ConflictStyle::setColor(Style style, const QColor& color) { StyleManager::getConflictStyle().set(style, color); }

const QColor& ConflictStyle::getColor(Style style) { return StyleManager::getConflictStyle().get(style); }

const QColor& ConflictStyle::getDefaultColor(Style style) {
    assert(style >= 0 && style < Style::_LENGTH);
    return default_colors[style];
}

void ConflictStyle::emitChangedSignal() { emit changed(); }

void ConflictStyle::emitChanged() { StyleManager::getConflictStyle().emitChangedSignal(); }

void ConflictStyle::save(QSettings& settings) const {
    settings.beginGroup("ConflictStyle");
    {
        for (std::size_t i = 0; i < STYLE_NAMES.size(); ++i) {
            auto style       = static_cast<Style>(i);
            const char* name = STYLE_NAMES[i];

            settings.setValue(name, getColor(style));
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

            setColor(style, color);
        }
    }
    settings.endGroup();
}

}
