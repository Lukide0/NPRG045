#include "gui/open_file.h"

#include <QDesktopServices>
#include <QUrl>

namespace gui {

bool open_file(const std::filesystem::path& path) {
    QUrl url = QUrl::fromLocalFile(QString::fromStdU32String(path.u32string()));
    return QDesktopServices::openUrl(url);
}

}
