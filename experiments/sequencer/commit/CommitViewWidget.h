#pragma once

#include "clear_layout.h"
#include <git2/types.h>
#include <QGridLayout>
#include <QLabel>
#include <QWidget>

class CommitViewWidget : public QWidget {

public:
    CommitViewWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        m_layout = new QGridLayout();
        setLayout(m_layout);
    }

    void update(const char* hash, const char* summary, const char* description, const char* autor, const char* date) {
        clear_layout(m_layout);

        create_row("Hash:", hash, 0);
        create_row("Author:", autor, 1);
        create_row("Date:", date, 2);
        create_row("Summary:", summary, 3);
        create_row("Description:", description, 4);
    }

private:
    QGridLayout* m_layout;

    static QLabel* create_label(const QString& text) { return new QLabel(text); }

    void create_row(const QString& label, const QString& value, int row) {
        m_layout->addWidget(create_label(label), row, 0);
        m_layout->addWidget(create_label(value), row, 1);
    }
};
