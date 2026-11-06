#pragma once

#include "core/Result.h"

#include <QList>
#include <QSettings>
#include <QString>

namespace app::conflict {

class ConflictEditor {
public:
    struct Editor {
        QString program;
        QStringList arguments;
    };

    struct EditorValues {
        QString base;
        QString our;
        QString their;
        QString result;
    };

    enum class ConflictStyle {
        DIFF3,
        ZDIFF3,
        MERGE,
    };

    void setEditor(QString editor);
    void setStyle(ConflictStyle style);

    [[nodiscard]] ConflictStyle style() const { return m_style; }

    void load(QSettings& settings);
    void save(QSettings& settings);

    [[nodiscard]] QString commandTemplate() const;

    static core::Result<Editor> validate(QString editor);

    core::Result<Editor> createEditor(const EditorValues& files) const;

private:
    QString m_template;
    ConflictStyle m_style = ConflictStyle::ZDIFF3;
};

}
