#pragma once

#include "Node.h"
#include <cstdint>
#include <functional>
#include <QGraphicsView>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qwidget.h>

class GraphWidget : public QGraphicsView {
public:
    GraphWidget(QWidget* parent = nullptr)
        : QGraphicsView(parent) {

        auto* scene = new QGraphicsScene(this);
        scene->setItemIndexMethod(QGraphicsScene::NoIndex);
        scene->setSceneRect(0, 0, 100, 100);
        setScene(scene);

        setRenderHint(QPainter::RenderHint::Antialiasing);
        setTransformationAnchor(AnchorUnderMouse);
        setAlignment(Qt::AlignLeft | Qt::AlignTop);
        setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
        setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

        scale(qreal(0.95), qreal(0.95));
        setViewportMargins(0, 0, 10, 0);
    }

    ~GraphWidget() override = default;

    Node* addNode(std::uint32_t y);
    Node* addNode();
    void clear();

    void setHandle(const std::function<void(Node*, Node*)>& handle) { m_handle = handle; }

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    std::uint32_t m_next_y                     = 0;
    std::function<void(Node*, Node*)> m_handle = defaultHandle;

    static void defaultHandle(Node* /*unused*/, Node* /*unused*/) { }
};
