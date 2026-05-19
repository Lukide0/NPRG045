#include "gui/style/load_style.h"
#include "logging/Log.h"

#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QStringList>

namespace gui::style {

bool load_style(QApplication& app, const QString& path) {
    QFile file(path);

    if (!file.exists()) {
        LOG_WARN("Stylesheet does not exists: {}", path.toStdString());
        return false;
    }

    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        LOG_WARN("Could not open stylesheet: {}", path.toStdString());
        return false;
    }

    const QByteArray data = file.readAll();
    if (data.isEmpty()) {
        LOG_WARN("Stylesheet is empty: {}", path.toStdString());
        return false;
    }

    app.setStyleSheet(QString::fromUtf8(data));
    return true;
}

bool load_font(const QString& path) {
    bool res = QFontDatabase::addApplicationFont(path) != -1;
    if (!res) {
        LOG_WARN("Could not load font: '{}'", path.toStdString());
    }

    return res;
}

bool load_fonts(const QStringList& paths) {
    bool res = true;
    for (auto&& path : paths) {
        res &= load_font(path);
    }

    return res;
}

bool load_and_set_font(const QString& path, int point_size) {
    int id = QFontDatabase::addApplicationFont(path);
    if (id == -1) {
        LOG_WARN("Could not load font: '{}'", path.toStdString());
        return false;
    }

    auto families = QFontDatabase::applicationFontFamilies(id);
    if (families.empty()) {
        LOG_WARN("Could not find font family for: '{}'", path.toStdString());
        return false;
    }

    QFont font(families.first());
    font.setPointSize(point_size);

    QApplication::setFont(font);

    return true;
}

}
