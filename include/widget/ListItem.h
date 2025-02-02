#pragma once

#include "graph/Node.h"
#include <QColor>
#include <QLabel>
#include <QListWidgetItem>
#include <QPalette>
#include <vector>

class ListItem : public QListWidgetItem {
public:
    using QListWidgetItem::QListWidgetItem;

    void addConnection(Node* item) { m_connected.push_back(item); }

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

    void setColorToAll(const QColor& color) {
        setBackground(color);

        for (auto* item : m_connected) {
            item->setFill(color);
        }
    }

private:
    std::vector<Node*> m_connected;
    QColor m_color;
};
