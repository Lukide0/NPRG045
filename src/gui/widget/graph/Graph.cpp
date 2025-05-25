#include "gui/widget/graph/Graph.h"
#include "gui/widget/graph/Node.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QScrollBar>

namespace gui::widget {

Node* GraphWidget::addNode(std::uint32_t y) {
    auto* node = new Node(this);
    scene()->addItem(node);

    auto node_height = node->boundingRect().height();
    auto node_width  = node->boundingRect().width();

    auto padding = 2;
    auto gap     = 2;
    auto pos_y   = y * (node_height + gap);

    node->setPos(padding, pos_y + gap);

    m_next_y = std::max<std::uint32_t>(y + 1, m_next_y);

    double new_height = ((y + 1) * (node_height + gap)) + gap;
    double new_width  = node_width + padding;

    double curr_height = sceneRect().height();
    double curr_width  = sceneRect().width();

    auto rect = sceneRect();
    rect.setHeight(std::max(new_height, curr_height));
    rect.setWidth(std::max(new_width, curr_width));

    setSceneRect(rect);

    m_nodes.push_back(node);

    return node;
}

Node* GraphWidget::addNode() { return addNode(m_next_y); }

Node* GraphWidget::find(std::function<bool(const Node*)> prec) {
    for (auto* node : m_nodes) {
        if (prec(node)) {
            return node;
        }
    }

    return nullptr;
}

void GraphWidget::clear() {

    m_nodes.clear();

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

}
