#pragma once

#include "gui/widget/graph/Node.h"
#include <QWidget>

class DiffWidget : public QWidget {
public:
    using QWidget::QWidget;

    void update(Node* node) { m_node = node; }

private:
    Node* m_node = nullptr;
};
