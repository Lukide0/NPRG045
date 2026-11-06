#include "gui/style/DiffStyle.h"
#include "gui/style/StyleManager.h"

#include <cassert>
#include <cstddef>

#include <QColor>
#include <QSettings>

namespace gui::style {

void DiffStyle::set(Style style, const QColor& color) {
    assert(style >= 0 && style < Style::_LENGTH);
    m_colors[style] = color;
}

[[nodiscard]] const QColor& DiffStyle::get(Style style) const {
    assert(style >= 0 && style < Style::_LENGTH);
    return m_colors[style];
}

void DiffStyle::setColor(Style style, const QColor& color) { StyleManager::getDiffStyle().set(style, color); }

const QColor& DiffStyle::getColor(Style style) { return StyleManager::getDiffStyle().get(style); }

const QColor& DiffStyle::getDefaultColor(Style style) {
    assert(style >= 0 && style < Style::_LENGTH);
    return default_colors[style];
}

void DiffStyle::emitChangedSignal() { emit changed(); }

void DiffStyle::emitChanged() { StyleManager::getDiffStyle().emitChangedSignal(); }

void DiffStyle::save(QSettings& settings) const {
    settings.beginGroup("DiffStyle");
    {
        for (std::size_t i = 0; i < STYLE_NAMES.size(); ++i) {
            auto style       = static_cast<Style>(i);
            const char* name = STYLE_NAMES[i];

            settings.setValue(name, getColor(style));
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

            setColor(style, color);
        }
    }
    settings.endGroup();
}
}
