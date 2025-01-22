#pragma once

#include <QColor>
#include <QLabel>
#include <QListWidgetItem>
#include <QPalette>
#include <vector>

class ListItem : public QListWidgetItem {
public:
    using QListWidgetItem::QListWidgetItem;

    void addConnection(QLabel* item) { m_connected.push_back(item); }

    void setItemColor(QColor color) { m_color = color; }

    [[nodiscard]] const QColor& getItemColor() const { return m_color; }

    static void handleChange(QListWidgetItem* curr) {
        if (curr != nullptr) {
            auto* item = dynamic_cast<ListItem*>(curr);
            if (item == nullptr) {
                return;
            }

            item->setColorToAll(item->m_color);
        }
    }

    [[nodiscard]] const auto& getConnected() const { return m_connected; }

    void setColorToAll(QColor color) {
        setBackground(color);
        QPalette pallete;
        pallete.setColor(QPalette::Window, color);
        pallete.setColor(QPalette::WindowText, Qt::black);

        for (auto* item : m_connected) {
            item->setAutoFillBackground(true);
            item->setPalette(pallete);
        }
    }

private:
    std::vector<QLabel*> m_connected;
    QColor m_color;
};
