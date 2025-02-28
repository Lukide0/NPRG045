#pragma once

#include "gui/clear_layout.h"
#include "gui/widget/graph/Node.h"
#include "gui/widget/NamedListWidget.h"
#include <ctime>
#include <git2/commit.h>
#include <git2/types.h>
#include <QGridLayout>
#include <QLabel>
#include <qobject.h>
#include <QWidget>

class CommitViewWidget : public QWidget {

public:
    CommitViewWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        m_layout = new QGridLayout();
        m_layout->setColumnStretch(0, 1);
        m_layout->setColumnStretch(1, 2);
        m_layout->setColumnStretch(2, 2);
        setLayout(m_layout);
    }

    void update(Node* node) {
        clear_layout(m_layout);

        m_node = node;

        create_rows();
        prepare_diff();
    }

private:
    QGridLayout* m_layout;
    NamedListWidget* m_changes;
    Node* m_node = nullptr;

    static QLabel* create_label(const QString& text) { return new QLabel(text); }

    void create_row(const QString& label, const QString& value, int row) {
        m_layout->addWidget(create_label(label), row, 0);
        m_layout->addWidget(create_label(value), row, 1);
    }

    void create_rows();
    void prepare_diff();
};
