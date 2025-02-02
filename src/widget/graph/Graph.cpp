#include "widget/graph/Graph.h"
#include "widget/graph/Node.h"

#include <algorithm>
#include <cstdint>
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>

Node* GraphWidget::addNode(std::uint32_t y) {
    auto* node = new Node(this);
    scene()->addItem(node);

    auto padding = 10;
    auto gap     = 10;
    auto pos_y   = y * (node->boundingRect().height() + gap);

    node->setPos(padding, pos_y + padding);

    m_next_y = std::max<std::uint32_t>(y + 1, m_next_y);

    return node;
}

Node* GraphWidget::addNode() { return addNode(m_next_y); }

void GraphWidget::clear() {
    for (auto* item : scene()->items()) {
        scene()->removeItem(item);
        delete item;
    }
    m_next_y = 0;
}

void GraphWidget::mousePressEvent(QMouseEvent* event) {
    // HACK: Rather than manually managing focus, retrieve the currently focused item, handle the mouse press, and
    // determine the new focused item.

    auto* old_item = scene()->focusItem();

    QGraphicsView::mousePressEvent(event);

    auto* new_item = scene()->focusItem();

    auto* old_node = dynamic_cast<Node*>(old_item);
    auto* new_node = dynamic_cast<Node*>(new_item);

    m_handle(old_node, new_node);
}
