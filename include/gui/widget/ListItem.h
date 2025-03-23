#pragma once

#include "core/git/parser.h"
#include "gui/widget/graph/Node.h"

#include <QBoxLayout>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QListWidgetItem>
#include <QPalette>
#include <QWidget>

#include <array>
#include <vector>

class ListItem : public QWidget {
public:
    static constexpr auto items = std::to_array<CmdType>({
        CmdType::PICK,
        CmdType::REWORD,
        CmdType::EDIT,
        CmdType::DROP,
        CmdType::FIXUP,
        CmdType::SQUASH,
    });

    static constexpr int indexOf(CmdType type) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (items[i] == type) {
                return i;
            }
        }

        return -1;
    }

    ListItem(QWidget* widget = nullptr)
        : QWidget(widget) {
        m_combo = new QComboBox();

        for (auto&& item : items) {
            m_combo->addItem(cmd_to_str(item), static_cast<int>(item));
        }

        m_text = new QLabel();
        m_text->setMargin(0);
        m_text->setContentsMargins(0, 0, 0, 0);

        m_layout = new QHBoxLayout();
        m_layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
        m_layout->addWidget(m_combo);
        m_layout->addWidget(m_text, 1);

        setLayout(m_layout);

        connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int index) {
            assert(index != -1);

            auto raw_type = m_combo->itemData(index).toInt();
            m_action.type = static_cast<CmdType>(raw_type);
        });
    }

    void setCommitAction(const CommitAction& action) {
        m_action = action;

        auto index = indexOf(action.type);
        assert(index != -1);

        m_combo->setCurrentIndex(index);
    }

    QComboBox* getComboBox() { return m_combo; }

    [[nodiscard]] const CommitAction& getCommitAction() const { return m_action; }

    void setNode(Node* node) { m_node = node; }

    void setText(const QString& text) { m_text->setText(text); }

    Node* getNode() { return m_node; }

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
        auto pal = palette();
        pal.setColor(backgroundRole(), color);
        setPalette(pal);

        for (auto* item : m_connected) {
            item->setFill(color);
        }
    }

private:
    Node* m_node = nullptr;
    std::vector<Node*> m_connected;
    CommitAction m_action;
    QColor m_color;

    QLabel* m_text;
    QHBoxLayout* m_layout;
    QComboBox* m_combo;
};
