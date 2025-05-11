#include "gui/widget/graph/Node.h"
#include "action/ActionManager.h"
#include "gui/color.h"
#include <QFontDatabase>
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

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    painter->setFont(font);

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

    painter->drawRect(10, 0, 58, 20);

    std::string msg = m_commit_msg;

    if (m_action != nullptr && m_action->has_msg()) {
        auto id = m_action->get_msg_id();
        if (id.is_value()) {
            msg = ActionsManager::get().get_msg(id.value());
        }
    }

    const char* hash = m_commit_hash.c_str();

    painter->drawText(QRectF { 12, 0, 56, 20 }, hash, text_opts);

    auto fm           = painter->fontMetrics();
    int size          = fm.horizontalAdvance(msg.c_str(), msg.size());
    int avg_char_size = fm.averageCharWidth();

    QString qmsg = QString::fromStdString(msg);

    if (size > 220) {
        int new_size = 220 / avg_char_size;
        new_size     = std::max(new_size - 3, 0);
        qmsg.resize(new_size);
        qmsg += "...";
    }

    painter->setPen(pen);
    painter->drawText(QRectF { 75, 0, 220, 20 }, qmsg, text_opts);
}
