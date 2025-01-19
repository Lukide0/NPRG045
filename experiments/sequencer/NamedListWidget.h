#pragma once

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

class NamedListWidget : public QWidget {
public:
    NamedListWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        m_layout = new QVBoxLayout();
        m_list   = new QListWidget();
        m_header = new QLabel();
        m_layout->addWidget(m_header);
        m_layout->addWidget(m_list);

        this->setLayout(m_layout);
    }

    NamedListWidget(QString header, QWidget* parent = nullptr)
        : QWidget(parent) {
        m_layout = new QVBoxLayout();
        m_list   = new QListWidget();
        m_header = new QLabel(header);

        m_layout->addWidget(m_header);
        m_layout->addWidget(m_list);

        this->setLayout(m_layout);
    }

    QListWidget* get_list() { return m_list; }

    void set_header(QString text) { m_header->setText(text); }

    void clear() { m_list->clear(); }

private:
    QLabel* m_header;
    QListWidget* m_list;
    QVBoxLayout* m_layout;
};
