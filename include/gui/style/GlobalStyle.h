#pragma once

#include <array>
#include <QColor>
#include <QObject>
#include <QSettings>

namespace gui::style {

class GlobalStyle : public QObject {
    Q_OBJECT
public:
    enum Style {
        HIGHLIGHT,
        _LENGTH,
    };

    static constexpr auto STYLE_NAMES = std::to_array<const char*>({
        "Highlight",
    });

    static_assert(STYLE_NAMES.size() == Style::_LENGTH);

    static constexpr std::array<QColor, Style::_LENGTH> default_colors {
        QColor { 84, 106, 123 },
    };

    void set(Style style, const QColor& color);
    [[nodiscard]] const QColor& get(Style style) const;

    static void set_color(Style style, const QColor& color);
    static const QColor& get_color(Style style);

    static const QColor& get_default_color(Style style);

    static void emit_changed_signal();

    void emit_changed();

    void save(QSettings& settings) const;
    void load(QSettings& settings);

signals:
    void changed();

private:
    std::array<QColor, Style::_LENGTH> m_colors = default_colors;
};

}
