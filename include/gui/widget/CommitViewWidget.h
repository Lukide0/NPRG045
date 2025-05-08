#pragma once

#include "gui/widget/CommitMessageWidget.h"
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
    CommitViewWidget(DiffWidget* diff, ActionsManager& manager);

    void update(Node* node) {
        m_parent_commit = nullptr;

        if (node == nullptr) {
            m_action = nullptr;
            m_commit = nullptr;

        } else {
            m_action = node->getAction();
            m_commit = node->getCommit();
            if (auto* parent = node->getParentNode()) {
                m_parent_commit = parent->getCommit();
            }
        }

        update();
    }

    void update() {
        createRows();

        m_diff->update(m_commit, m_parent_commit);

        prepareDiff();
    }

private:
    QHBoxLayout* m_layout;
    QFormLayout* m_info_layout;
    NamedListWidget* m_changes;
    CommitMessageWidget* m_msg;

    ActionsManager& m_manager;
    DiffWidget* m_diff;
    Action* m_action            = nullptr;
    git_commit* m_parent_commit = nullptr;
    git_commit* m_commit        = nullptr;

    static QLabel* create_label(const QString& text) { return new QLabel(text); }

    void createRows();
    void prepareDiff();
};
