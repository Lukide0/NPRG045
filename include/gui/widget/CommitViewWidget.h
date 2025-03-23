#pragma once

#include "gui/widget/DiffWidget.h"
#include "gui/widget/graph/Node.h"
#include "gui/widget/NamedListWidget.h"
#include <ctime>
#include <git2/commit.h>
#include <git2/types.h>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <qobject.h>
#include <QWidget>

class CommitViewWidget : public QWidget {

public:
    CommitViewWidget(DiffWidget* diff)
        : m_diff(diff) {

        m_layout = new QHBoxLayout();
        setLayout(m_layout);

        m_info_layout = new QFormLayout();
        m_layout->addLayout(m_info_layout, 2);

        m_changes = new NamedListWidget("Changes");
        m_layout->addWidget(m_changes);

        createRows();
    }

    void update(Node* node) {
        m_node = node;

        createRows();

        m_diff->update(node);
        prepareDiff();
    }

private:
    QHBoxLayout* m_layout;
    QFormLayout* m_info_layout;
    NamedListWidget* m_changes;
    DiffWidget* m_diff;
    Node* m_node = nullptr;

    static QLabel* create_label(const QString& text) { return new QLabel(text); }

    void createRows();
    void prepareDiff();
};
