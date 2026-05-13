#include "gui/style/load_style.h"
#include "logging/Log.h"

#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QString>

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

}
