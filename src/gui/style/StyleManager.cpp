#include "gui/style/StyleManager.h"

namespace gui::style {

StyleManager& StyleManager::get() {
    static StyleManager instance;

    return instance;
}

}
