#include "gui/window/PreferencesWindow.h"

#include <QHBoxLayout>
#include <QLabel>

namespace gui::window {

PreferencesWindow::PreferencesWindow() {
    m_layout = new QHBoxLayout();

    auto* main = new QWidget(this);
    main->setContentsMargins(0, 0, 0, 0);
    setCentralWidget(main);

    main->setLayout(m_layout);
}

}
