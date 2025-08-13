#pragma once

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

QString get_temp_filepath(const QString& filename);

std::pair<QString, bool> create_temp_file(const QString& text, const QString& filename, bool open = true);
bool open_temp_file(const QString& filepath);

std::pair<QString, bool> read_temp_file(const QString& filename);
std::pair<QString, bool> read_file(const QString& filepath);

}
