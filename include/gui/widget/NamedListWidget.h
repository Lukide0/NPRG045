#pragma once

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QWidget>

class NamedListWidget : public QWidget {
public:
    NamedListWidget(QWidget* parent = nullptr)
        : QWidget(parent) {

        m_layout = new QVBoxLayout();
        m_list   = new QListWidget();
        m_header = new QLabel();
        m_layout->addWidget(m_header);
        m_layout->addWidget(m_list);
        m_layout->setContentsMargins(0, 0, 0, 0);

        setLayout(m_layout);
        setContentsMargins(0, 0, 0, 0);

        m_list->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
    }

    NamedListWidget(QString header, QWidget* parent = nullptr)
        : QWidget(parent) {
        m_layout = new QVBoxLayout();
        m_list   = new QListWidget();
        m_header = new QLabel(header);

        m_layout->addWidget(m_header);
        m_layout->addWidget(m_list);
        m_layout->setContentsMargins(0, 0, 0, 0);

        setLayout(m_layout);
        setContentsMargins(0, 0, 0, 0);

        m_list->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
    }

    QListWidget* getList() { return m_list; }

    void setHeader(QString text) { m_header->setText(text); }

    void clear() { m_list->clear(); }

    void enableDrag() {
        m_list->setDragEnabled(true);
        m_list->setDragDropMode(QAbstractItemView::DragDropMode::InternalMove);
    }

private:
    QLabel* m_header;
    QListWidget* m_list;
    QVBoxLayout* m_layout;
};
