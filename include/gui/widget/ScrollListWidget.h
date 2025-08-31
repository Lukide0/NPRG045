#pragma once

#include <QListWidget>
#include <QObject>
#include <QTimer>
#include <QWidget>

namespace gui::widget {
class ScrollListWidget : public QListWidget {
    Q_OBJECT

public:
    explicit ScrollListWidget(QWidget* parent = nullptr);

protected:
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void autoScroll();

private:
    static constexpr int SCROLL_MARGIN   = 20;
    static constexpr int SCROLL_SPEED_MS = 100;

    enum ScrollDirection {
        UP   = -1,
        DOWN = 1,
        NONE = 0,
    };

    QTimer* m_timer;
    ScrollDirection m_dir;
};

}
