#include "gui/style/StyleManager.h"
#include "App.h"

#include <QSettings>

namespace gui::style {

StyleManager& StyleManager::get() {
    static StyleManager instance;

    return instance;
}

void StyleManager::load_styles(QSettings& settings) {
    m_conflict_style.load(settings);
    m_diff_style.load(settings);
}

void StyleManager::save_styles(QSettings& settings) const {
    m_conflict_style.save(settings);
    m_diff_style.save(settings);
}

}
