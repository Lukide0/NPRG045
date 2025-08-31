#include "gui/widget/ScrollListWidget.h"

#include <QDragMoveEvent>
#include <QScrollBar>
#include <QTimer>

namespace gui::widget {

ScrollListWidget::ScrollListWidget(QWidget* parent)
    : QListWidget(parent)
    , m_dir(ScrollDirection::NONE) {

    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    m_timer->setInterval(SCROLL_SPEED_MS);
    connect(m_timer, &QTimer::timeout, this, &ScrollListWidget::autoScroll);
}

void ScrollListWidget::dragMoveEvent(QDragMoveEvent* event) {
    QListWidget::dragMoveEvent(event);

    const QRect widget_rect = rect();
    const QPoint pos        = event->position().toPoint();

    if (pos.y() < SCROLL_MARGIN) {
        m_dir = ScrollDirection::UP;
        if (!m_timer->isActive()) {
            m_timer->start();
        }
    } else if (pos.y() > widget_rect.height() - SCROLL_MARGIN) {
        m_dir = ScrollDirection::DOWN;
        if (!m_timer->isActive()) {
            m_timer->start();
        }
    } else {
        m_dir = ScrollDirection::NONE;
        m_timer->stop();
    }
}

void ScrollListWidget::dragLeaveEvent(QDragLeaveEvent* event) {
    QListWidget::dragLeaveEvent(event);
    m_timer->stop();
    m_dir = ScrollDirection::NONE;
}

void ScrollListWidget::dropEvent(QDropEvent* event) {
    m_timer->stop();
    m_dir = ScrollDirection::NONE;
    QListWidget::dropEvent(event);
}

void ScrollListWidget::autoScroll() {
    if (m_dir == ScrollDirection::NONE) {
        m_timer->stop();
        return;
    }

    QScrollBar* scrollbar = verticalScrollBar();
    if (scrollbar == nullptr) {
        return;
    }

    int curr_value = scrollbar->value();
    int value      = curr_value + (m_dir * scrollbar->singleStep());

    value = qBound(scrollbar->minimum(), value, scrollbar->maximum());

    if (value != curr_value) {
        scrollbar->setValue(value);
    } else {
        m_timer->stop();
        m_dir = ScrollDirection::NONE;
    }
}

}
