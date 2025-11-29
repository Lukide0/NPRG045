#pragma once

#include "gui/style/ConflictStyle.h"
#include "gui/style/DiffStyle.h"

namespace gui::style {

class StyleManager {
public:
    static StyleManager& get();

    void load_styles(QSettings& settings);
    void save_styles(QSettings& settings) const;

    static DiffStyle& get_diff_style() { return get().m_diff_style; }

    static ConflictStyle& get_conflict_style() { return get().m_conflict_style; }

    [[nodiscard]] const DiffStyle& diff_style() const { return m_diff_style; }

    DiffStyle& diff_style() { return m_diff_style; }

    [[nodiscard]] const ConflictStyle& conflict_style() const { return m_conflict_style; }

    ConflictStyle& conflict_style() { return m_conflict_style; }

private:
    StyleManager() = default;

    DiffStyle m_diff_style;
    ConflictStyle m_conflict_style;
};

}
