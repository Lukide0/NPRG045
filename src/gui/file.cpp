#include "gui/file.h"

#include <utility>

#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QString>
#include <QStringConverter>
#include <QTextStream>
#include <QUrl>

namespace gui {

QString get_temp_filepath(const QString& filename) {
    QString temp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);

    const QString app_name = QCoreApplication::applicationName().isEmpty() ? QStringLiteral("GitShuffle")
                                                                           : QCoreApplication::applicationName();

    const QString app_temp_dir = QDir(temp_dir).filePath(app_name);

    if (!QDir().mkpath(app_temp_dir)) {
        return { };
    }

    return QDir(app_temp_dir).filePath(filename);
}

bool open_temp_file(const QString& filepath) { return QDesktopServices::openUrl(QUrl::fromLocalFile(filepath)); }

std::pair<QString, bool> create_temp_file(const QString& text, const QString& filename, bool open) {
    const QString filepath = get_temp_filepath(filename);

    if (filepath.isEmpty()) {
        return { filepath, false };
    }

    QFile file(filepath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return { filepath, false };
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << text;

    file.close();

    if (open) {
        return { filepath, open_temp_file(filepath) };
    }

    return { filepath, true };
}

std::pair<QString, bool> read_file(const QString& filepath) {
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::make_pair(QString(), false);
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString content = in.readAll();
    file.close();

    return std::make_pair(content, true);
}

std::pair<QString, bool> read_temp_file(const QString& filename) { return read_file(get_temp_filepath(filename)); }

}
