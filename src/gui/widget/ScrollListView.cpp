#include "gui/widget/ScrollListView.h"

#include <QDragMoveEvent>
#include <QListWidget>
#include <QPoint>
#include <QRect>
#include <QScrollBar>
#include <QTimer>
#include <QtMinMax>
#include <QWidget>

namespace gui {

ScrollListView::ScrollListView(QWidget* parent)
    : QListView(parent)
    , m_timer(new QTimer(this)) {
    setAutoScroll(false);

    m_timer->setInterval(SCROLL_SPEED_MS);
    connect(m_timer, &QTimer::timeout, this, &ScrollListView::autoScroll);
}

ScrollListView::ScrollDirection ScrollListView::directionForPosition(int y) const {
    if (y < SCROLL_MARGIN) {
        return ScrollDirection::UP;
    }

    if (y > viewport()->height() - SCROLL_MARGIN) {
        return ScrollDirection::DOWN;
    }

    return ScrollDirection::NONE;
}

void ScrollListView::updateTimer() {
    if (m_dir == ScrollDirection::NONE) {
        m_timer->stop();
        return;
    }

    if (!m_timer->isActive()) {
        m_timer->start();
    }
}

void ScrollListView::dragMoveEvent(QDragMoveEvent* event) {
    QListView::dragMoveEvent(event);

    m_dir = directionForPosition(event->position().toPoint().y());
    updateTimer();
}

void ScrollListView::dragLeaveEvent(QDragLeaveEvent* event) {
    QListView::dragLeaveEvent(event);

    m_dir = ScrollDirection::NONE;
    updateTimer();
}

void ScrollListView::dropEvent(QDropEvent* event) {
    // Stop first: the drop may open a dialog with a nested event loop
    m_dir = ScrollDirection::NONE;
    updateTimer();

    QListView::dropEvent(event);
}

void ScrollListView::autoScroll() {
    QScrollBar* scrollbar = verticalScrollBar();
    if (m_dir == ScrollDirection::NONE || scrollbar == nullptr) {
        m_timer->stop();
        return;
    }

    const int current = scrollbar->value();
    const int target  = qBound(scrollbar->minimum(), current + (m_dir * scrollbar->singleStep()), scrollbar->maximum());

    if (target == current) {
        m_timer->stop();
        return;
    }

    scrollbar->setValue(target);
}
}
