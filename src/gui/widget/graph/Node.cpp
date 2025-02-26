#include "gui/widget/graph/Node.h"
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

QRectF Node::boundingRect() const { return { 0, 0, 300, 40 }; }

QPainterPath Node::shape() const {
    QPainterPath path;
    path.addRect(0, 0, 300, 40);
    return path;
}

void Node::setFill(const QColor& color) {
    m_fill = color;
    update();
}

void Node::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {

    if (hasConflict()) {
        auto brushStatus = QBrush(Qt::yellow);
        painter->setBrush(brushStatus);
        painter->drawRect(0, 13, 14, 14);
    }

    QFont font = painter->font();

    auto pen     = QPen(Qt::black);
    auto penRect = QPen(Qt::black);
    painter->setBrush(m_fill);

    auto rectOpts = QTextOption(Qt::AlignHCenter | Qt::AlignVCenter);

    if (hasFocus()) {
        penRect.setWidth(2);
        font.setBold(true);
    }

    painter->setFont(font);

    painter->setPen(penRect);

    painter->drawRoundedRect(20, 0, 80, 40, 10, 10, Qt::SizeMode::AbsoluteSize);
    painter->drawText(QRectF { 25, 5, 70, 30 }, m_hash.c_str(), rectOpts);

    painter->setPen(pen);
    painter->drawText(QRectF { 120, 5, 180, 30 }, m_msg.c_str(), QTextOption(Qt::AlignLeft | Qt::AlignVCenter));
}
