#pragma once

#include <QLabel>
#include <QWidget>

namespace gui::widget {

class SLabel : public QLabel {
public:
    SLabel(QWidget* parent = nullptr)
        : QLabel(parent) {
        init();
    }

    SLabel(const QString& text, QWidget* parent = nullptr)
        : QLabel(text, parent) {
        init();
    }

private:
    void init() { setTextInteractionFlags(textInteractionFlags() | Qt::TextInteractionFlag::TextSelectableByMouse); }
};

}
