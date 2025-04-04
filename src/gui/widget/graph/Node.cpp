#include "gui/widget/graph/Node.h"
#include "gui/color.h"
#include <QGraphicsItem>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

QRectF Node::boundingRect() const { return { 0, 0, 300, 20 }; }

QPainterPath Node::shape() const {
    QPainterPath path;
    path.addRect(0, 0, 300, 20);
    return path;
}

void Node::setFill(const QColor& color) {
    m_fill = color;
    update();
}

void Node::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {

    if (hasConflict()) {
        auto brush = QBrush(convert_to_color(ColorType::DELETION));
        painter->setBrush(brush);
        painter->setPen(Qt::PenStyle::NoPen);
        painter->drawRect(0, 5, 8, 8);
    }

    QFont font = painter->font();

    auto pen      = QPen(Qt::black);
    auto pen_rect = QPen(Qt::black);
    painter->setBrush(m_fill);

    auto text_opts = QTextOption(Qt::AlignLeft | Qt::AlignVCenter);

    if (hasFocus()) {
        pen_rect.setWidth(2);
        font.setBold(true);
    }

    painter->setFont(font);

    painter->setPen(pen_rect);

    painter->drawRect(10, 0, 80, 20);
    painter->drawText(QRectF { 12, 0, 76, 20 }, m_hash.c_str(), text_opts);

    auto fm           = painter->fontMetrics();
    int size          = fm.horizontalAdvance(m_msg.c_str());
    int avg_char_size = fm.averageCharWidth();

    QString msg = QString::fromStdString(m_msg);

    if (size > 200) {
        int new_size = 200 / avg_char_size;
        new_size     = std::max(new_size - 3, 0);
        msg.resize(new_size);
        msg += "...";
    }

    painter->setPen(pen);
    painter->drawText(QRectF { 95, 0, 200, 20 }, msg, text_opts);
}
