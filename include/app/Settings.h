#pragma once

#include "build.h"
#include <QSettings>

namespace app {

class Settings {
public:
    static QSettings get() { return { QSettings::IniFormat, QSettings::UserScope, build::app_name }; }
};

}
