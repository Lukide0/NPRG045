#include "gui/file.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QUrl>
#include <utility>

namespace gui {

QString get_temp_filepath(const QString& filename) {
    QString temp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return QDir(temp_dir).filePath(filename);
}

bool open_temp_file(const QString& filepath) { return QDesktopServices::openUrl(QUrl::fromLocalFile(filepath)); }

std::pair<QString, bool> create_temp_file(const QString& text, const QString& filename, bool open) {
    QString filepath = get_temp_filepath(filename);

    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return std::make_pair(filepath, false);
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << text;
    file.close();

    if (open) {
        return std::make_pair(filepath, QDesktopServices::openUrl(QUrl::fromLocalFile(filepath)));
    }

    return std::make_pair(filepath, true);
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
