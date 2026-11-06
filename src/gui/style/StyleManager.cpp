#include "gui/style/StyleManager.h"

#include <QSettings>

namespace gui::style {

StyleManager& StyleManager::get() {
    static StyleManager instance;

    return instance;
}

void StyleManager::loadStyles(QSettings& settings) {
    m_global_style.load(settings);
    m_conflict_style.load(settings);
    m_diff_style.load(settings);
}

void StyleManager::saveStyles(QSettings& settings) const {
    m_global_style.save(settings);
    m_conflict_style.save(settings);
    m_diff_style.save(settings);
}

}
