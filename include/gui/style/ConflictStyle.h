#pragma once

#include <array>
#include <QColor>

namespace gui::style {

class ConflictStyle {
public:
    enum Style {
        NORMAL,
        UNKNOWN,
        CONFLICT,
        RESOLVED_CONFLICT,
        _LENGTH,
    };

    void set(Style style, const QColor& color);
    [[nodiscard]] const QColor& get(Style style) const;

    static void set_color(Style style, const QColor& color);
    static const QColor& get_color(Style style);

private:
    std::array<QColor, Style::_LENGTH> m_colors {
        QColor {},
        { 233, 196, 106 },
        { 239, 91, 111 },
        { 128, 26, 134 },
    };
};

}
