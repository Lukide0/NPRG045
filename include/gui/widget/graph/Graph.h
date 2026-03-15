#pragma once

#include "Node.h"
#include <cassert>
#include <cstdint>
#include <functional>
#include <QGraphicsView>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qwidget.h>

namespace gui::widget {

class GraphWidget : public QGraphicsView {
public:
    GraphWidget(QWidget* parent = nullptr);

    ~GraphWidget() override = default;

    Node* addNode(std::uint32_t y);
    Node* addNode();
    void clear();

    Node* nodeAt(int i) {
        assert(i >= 0 && i < static_cast<int>(m_nodes.size()));
        return m_nodes[i];
    }

    Node* find(std::function<bool(const Node*)> prec);

    void setHandle(const std::function<void(Node*, Node*)>& handle) { m_handle = handle; }

protected:
    void mousePressEvent(QMouseEvent* event) override;

    void resizeEvent(QResizeEvent* event) override;

private:
    std::uint32_t m_next_y                     = 0;
    std::function<void(Node*, Node*)> m_handle = defaultHandle;

    std::vector<Node*> m_nodes;

    static void defaultHandle(Node* /*unused*/, Node* /*unused*/) { }
};

}
