#pragma once

#include <array>
#include <QColor>
#include <QObject>
#include <QSettings>

namespace gui::style {

class ConflictStyle : public QObject {
    Q_OBJECT

public:
    enum Style {
        NORMAL            = 0,
        UNKNOWN           = 1,
        CONFLICT          = 2,
        RESOLVED_CONFLICT = 3,
        _LENGTH,
    };

    static constexpr auto STYLE_NAMES = std::to_array<const char*>({
        "Normal",
        "Unknown",
        "Conflict",
        "ResolvedConflict",
    });

    static_assert(STYLE_NAMES.size() == Style::_LENGTH);

    static constexpr std::array<QColor, Style::_LENGTH> default_colors {
        QColor { },
        { 233, 196, 106 },
        { 239, 91, 111 },
        { 128, 26, 134 },
    };

    void set(Style style, const QColor& color);
    [[nodiscard]] const QColor& get(Style style) const;

    static void setColor(Style style, const QColor& color);
    static const QColor& getColor(Style style);

    static const QColor& getDefaultColor(Style style);

    static void emitChanged();

    void emitChangedSignal();

    void save(QSettings& settings) const;
    void load(QSettings& settings);

signals:
    void changed();

private:
    std::array<QColor, Style::_LENGTH> m_colors = default_colors;
};

}
