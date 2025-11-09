#pragma once

#include <array>
#include <QColor>

namespace gui::style {

class DiffStyle {
public:
    enum Style {
        NORMAL,
        INFO,
        DELETION,
        ADDITION,
        _LENGTH,
    };

    void set(Style style, const QColor& color);
    [[nodiscard]] const QColor& get(Style style) const;

    static void set_color(Style style, const QColor& color);
    static const QColor& get_color(Style style);

private:
    std::array<QColor, Style::_LENGTH> m_colors {
        QColor {},
         { 44, 7, 156 },
         { 239, 91, 111 },
         { 6, 214, 160 }
    };
};

}
