#include "gui/widget/graph/Node.h"
#include "action/ActionManager.h"
#include "gui/color.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QGraphicsItem>
#include <qnamespace.h>
#include <QPainter>
#include <QPen>
#include <QString>
#include <QStyleOptionGraphicsItem>
#include <QTextOption>
#include <QWidget>

namespace gui::widget {

QRectF Node::boundingRect() const { return { 0, 0, m_width, HEIGHT }; }

QPainterPath Node::shape() const {
    QPainterPath path;
    path.addRect(0, 0, m_width, HEIGHT);
    return path;
}

void Node::setFill(const QColor& color) {
    m_fill = color;
    update();
}

void Node::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/) {

    constexpr auto CONFLICT_SIZE  = static_cast<int>((HEIGHT / 2.0) * 0.75);
    constexpr int CONFLICT_OFFSET = static_cast<int>((HEIGHT - CONFLICT_SIZE) / 2.0);
    constexpr int PADDING         = 2;
    constexpr int HASH_BOX_SIZE   = 58;

    if (hasConflict()) {
        auto brush = QBrush(convert_to_color(ColorType::DELETION));
        painter->setBrush(brush);
        painter->setPen(Qt::PenStyle::NoPen);
        painter->drawRect(0, CONFLICT_OFFSET, CONFLICT_SIZE, CONFLICT_SIZE);
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

    painter->drawRect(CONFLICT_SIZE + PADDING, 0, HASH_BOX_SIZE, HEIGHT);

    std::string msg = m_commit_msg;

    if (m_action != nullptr && m_action->has_msg()) {
        auto id = m_action->get_msg_id();
        if (id.is_value()) {
            msg = action::ActionsManager::get().get_msg(id.value());

            std::size_t index = msg.find('\n');
            if (index != std::string::npos) {
                msg = msg.substr(0, index);
            }
        }
    }

    const char* hash = m_commit_hash.c_str();

    painter->drawText(
        QRectF {
            CONFLICT_SIZE + PADDING + PADDING,
            0,
            HASH_BOX_SIZE - PADDING,
            HEIGHT,
        },
        hash,
        text_opts
    );

    auto fm           = painter->fontMetrics();
    int size          = fm.horizontalAdvance(msg.c_str(), static_cast<int>(msg.size()));
    int avg_char_size = fm.averageCharWidth();

    QString qmsg = QString::fromStdString(msg);

    constexpr int TEXT_OFFSET = CONFLICT_SIZE + HASH_BOX_SIZE + (2 * PADDING) + PADDING;

    const qreal text_size = m_width - TEXT_OFFSET;

    if (size > text_size) {
        int new_size = static_cast<int>(text_size / avg_char_size);
        new_size     = std::max(new_size - 6, 0);
        qmsg.resize(new_size);
        qmsg += "...";
    }

    painter->setPen(pen);
    painter->drawText(QRectF { TEXT_OFFSET, 0, text_size, HEIGHT }, qmsg, text_opts);
}

}
