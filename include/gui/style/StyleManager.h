#pragma once

#include "gui/style/ConflictStyle.h"
#include "gui/style/DiffStyle.h"
#include "gui/style/GlobalStyle.h"

#include <QSettings>

namespace gui::style {

class StyleManager {
public:
    static StyleManager& get();

    void loadStyles(QSettings& settings);
    void saveStyles(QSettings& settings) const;

    static GlobalStyle& getGlobalStyle() { return get().m_global_style; }

    static DiffStyle& getDiffStyle() { return get().m_diff_style; }

    static ConflictStyle& getConflictStyle() { return get().m_conflict_style; }

    [[nodiscard]] const DiffStyle& diffStyle() const { return m_diff_style; }

    DiffStyle& diffStyle() { return m_diff_style; }

    [[nodiscard]] const ConflictStyle& conflictStyle() const { return m_conflict_style; }

    ConflictStyle& conflictStyle() { return m_conflict_style; }

    [[nodiscard]] const GlobalStyle& globalStyle() const { return m_global_style; }

    GlobalStyle& globalStyle() { return m_global_style; }

private:
    StyleManager() = default;

    DiffStyle m_diff_style;
    ConflictStyle m_conflict_style;
    GlobalStyle m_global_style;
};

}
