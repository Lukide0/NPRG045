#include "gui/widget/graph/Graph.h"
#include "gui/widget/graph/Node.h"

#include <algorithm>
#include <cstdint>
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
#include <QScrollBar>

Node* GraphWidget::addNode(std::uint32_t y) {
    auto* node = new Node(this);
    scene()->addItem(node);

    auto node_height = node->boundingRect().height();

    auto padding = 10;
    auto gap     = 10;
    auto pos_y   = y * (node_height + gap);

    node->setPos(padding, pos_y + gap);

    m_next_y = std::max<std::uint32_t>(y + 1, m_next_y);

    double new_height  = ((y + 1) * (node_height + gap)) + gap;
    double curr_height = sceneRect().height();

    auto rect = sceneRect();
    rect.setHeight(std::max(new_height, curr_height));

    setSceneRect(rect);

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
