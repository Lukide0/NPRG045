#pragma once

#include <QLayout>
#include <QWidget>

inline void clear_layout(QLayout* layout) {
    if (layout == nullptr) {
        return;
    }

    while (auto* child = layout->takeAt(0)) {
        if (auto* w = child->widget()) {
            w->deleteLater();
        } else if (auto* l = child->layout()) {
            clear_layout(l);
        }

        delete child;
    }
}
