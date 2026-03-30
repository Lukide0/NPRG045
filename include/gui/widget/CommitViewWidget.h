#pragma once

#include "action/Action.h"
#include "action/ActionManager.h"
#include "gui/widget/CommitMessageWidget.h"
#include "gui/widget/DiffWidget.h"
#include "gui/widget/graph/Node.h"
#include "gui/widget/NamedListWidget.h"

#include <git2/commit.h>
#include <git2/types.h>

#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QWidget>

namespace gui::widget {

class CommitViewWidget : public QWidget {

public:
    CommitViewWidget(DiffWidget* diff);

    void update(Node* node);

    void update();

private:
    QHBoxLayout* m_layout;
    QFormLayout* m_info_layout;
    NamedListWidget* m_changes;
    CommitMessageWidget* m_msg;

    action::ActionsManager& m_manager;
    DiffWidget* m_diff;
    Node* m_node             = nullptr;
    action::Action* m_action = nullptr;
    git_commit* m_commit     = nullptr;

    void createRows();
    void prepareDiff();

    void updateInfo() {
        createRows();
        prepareDiff();
    }
};

}
