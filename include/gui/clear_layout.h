#pragma once

#include <QFormLayout>
#include <QLayout>
#include <QLayoutItem>
#include <QWidget>

namespace gui {

void clear_layout(QLayout* layout);

inline void clear_layout_item(QLayoutItem* item) {
    if (item == nullptr) {
        return;
    }

    if (auto* w = item->widget()) {
        w->hide();
        w->deleteLater();
    } else if (auto* l = item->layout()) {
        clear_layout(l);
        delete l;
    }
}

inline void clear_layout(QLayout* layout) {
    if (layout == nullptr) {
        return;
    }

    while (auto* child = layout->takeAt(0)) {
        clear_layout_item(child);
    }
}

inline void clear_layout(QFormLayout* layout) {
    if (layout == nullptr) {
        return;
    }

    while (layout->rowCount() > 0) {
        auto res = layout->takeRow(0);

        clear_layout_item(res.labelItem);
        clear_layout_item(res.fieldItem);
    }
}

}
