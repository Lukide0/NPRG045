#pragma once

#include "action/Action.h"
#include "action/ActionManager.h"
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

namespace gui::widget {

class CommitViewWidget : public QWidget {

public:
    CommitViewWidget(DiffWidget* diff);

    void update(Node* node) {
        m_node          = node;
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

        update_widget();
    }

    void update() {
        // NOTE: The node can be invalid here.
        m_node = nullptr;

        update_widget();
    }

private:
    QHBoxLayout* m_layout;
    QFormLayout* m_info_layout;
    NamedListWidget* m_changes;
    CommitMessageWidget* m_msg;

    action::ActionsManager& m_manager;
    DiffWidget* m_diff;
    Node* m_node                = nullptr;
    action::Action* m_action    = nullptr;
    git_commit* m_parent_commit = nullptr;
    git_commit* m_commit        = nullptr;

    static QLabel* create_label(const QString& text) { return new QLabel(text); }

    void createRows();
    void prepareDiff();

    void update_widget() {
        createRows();
        m_diff->update(m_commit, m_parent_commit);

        prepareDiff();
    }
};

}
