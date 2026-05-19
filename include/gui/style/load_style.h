#pragma once

#include <QApplication>
#include <QString>
#include <QStringList>

namespace gui::style {

bool load_style(QApplication& app, const QString& path);
bool load_fonts(const QStringList& paths);
bool load_font(const QString& path);
bool load_and_set_font(const QString& path, int point_size = 10);

}
