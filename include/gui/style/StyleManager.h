#pragma once

#include "gui/style/DiffStyle.h"

namespace gui::style {

class StyleManager {
public:
    static StyleManager& get();

    static DiffStyle& get_diff_style() { return get().m_diff_style; }

    [[nodiscard]] const DiffStyle& diff_style() const { return m_diff_style; }

    DiffStyle& diff_style() { return m_diff_style; }

private:
    StyleManager() = default;

    DiffStyle m_diff_style;
};

}
