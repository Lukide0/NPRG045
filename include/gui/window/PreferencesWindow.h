#pragma once

#include <QHBoxLayout>
#include <QMainWindow>

namespace gui::window {

class PreferencesWindow : public QMainWindow {
public:
    PreferencesWindow();

private:
    QHBoxLayout* m_layout;
};

}
