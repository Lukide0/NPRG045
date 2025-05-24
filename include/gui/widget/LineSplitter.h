#pragma once

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QSplitter>
#include <QSplitterHandle>

namespace gui::widget {

class LineSplitterHandle : public QSplitterHandle {
public:
    static constexpr int size = 5;

    LineSplitterHandle(Qt::Orientation orientation, QSplitter* parent)
        : QSplitterHandle(orientation, parent)
        , m_pen(Qt::black, 0.5) {
        if (orientation == Qt::Horizontal) {
            setFixedWidth(size);
        } else {
            setFixedHeight(size);
        }
    }

    void setLineWidth(int width) { m_pen.setWidth(width); }

    void setLineWidth(qreal width) { m_pen.setWidthF(width); }

    void setColor(const QColor& color) { m_pen.setColor(color); }

protected:
    void paintEvent(QPaintEvent* /*unused*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(m_pen);

        const auto w = width();
        const auto h = height();

        if (orientation() == Qt::Orientation::Horizontal) {
            painter.drawLine(w / 2, 0, w / 2, h);
        } else {
            painter.drawLine(0, h / 2, w, h / 2);
        }
    }

private:
    QPen m_pen;
};

class LineSplitter : public QSplitter {
public:
    using QSplitter::QSplitter;

    void setHandleWidth(int width) {
        for (int i = 0; i < count(); ++i) {
            if (auto* h = dynamic_cast<LineSplitterHandle*>(handle(i))) {
                h->setLineWidth(width);
            }
        }

        update();
    }

    void setHandleColor(const QColor& color) {
        for (int i = 0; i < count(); ++i) {
            if (auto* h = dynamic_cast<LineSplitterHandle*>(handle(i))) {
                h->setColor(color);
            }
        }
    }

protected:
    QSplitterHandle* createHandle() override { return new LineSplitterHandle(orientation(), this); }
};

}
