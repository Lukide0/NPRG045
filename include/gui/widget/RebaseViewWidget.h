#pragma once

#include "app/conflict/ConflictEditorManager.h"
#include "app/rebase/RebaseController.h"
#include "core/action/Action.h"
#include "core/git/GitGraph.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "gui/git/Graph.h"
#include "gui/git/GraphPresenter.h"
#include "gui/rebase/RebaseActionDelegate.h"
#include "gui/widget/CommitViewWidget.h"
#include "gui/widget/ConflictWidget.h"
#include "gui/widget/DiffWidget.h"

#include <memory>

#include <git2/oid.h>
#include <git2/types.h>

#include <QBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QObject>
#include <QPushButton>
#include <QSplitter>
#include <QStackedLayout>
#include <QWidget>

namespace gui::widget {

class RebaseViewWidget : public QWidget {
    Q_OBJECT
public:
    using GitGraph = core::git::GitGraph<void>;

    RebaseViewWidget(QWidget* parent = nullptr);

    void setSession(app::rebase::RebaseController* controller, GitGraph&& graph);

    void hideOldCommits() const { m_graphs.old->hide(); }

    void hideResultCommits() const { m_graphs.result->hide(); }

    void showResultCommits() const { m_graphs.result->show(); }

    void showOldCommits() const { m_graphs.old->show(); }

    QListView* getList() { return m_list; }

    void moveActionDown() { moveSelectedAction(true); }

    void moveActionUp() { moveSelectedAction(false); }

    void moveSelectedAction(bool down);

    void changeActionType();
    void changeActionType(core::action::ActionType type);

    core::rebase::RebaseAnalysisPtr rebaseAnalysis() const { return m_controller->analysis(); }

signals:
    void currentStepChanged(core::rebase::RebaseAnalysisConstPtr analysis, int row);

private:
    QPersistentModelIndex m_current;

    core::rebase::StepConflict* m_current_conflict = nullptr;
    app::rebase::RebaseController* m_controller    = nullptr;
    app::conflict::ConflictEditorManager* m_conflict_editor_manager;

    QListView* m_list;
    rebase::RebaseActionDelegate* m_action_delegate;

    struct {
        git::GraphPresenter* presenter;
        git::GraphWidget* old;
        git::GraphWidget* result;
    } m_graphs;

    CommitViewWidget* m_commit_view;
    DiffWidget* m_diff_widget;
    ConflictWidget* m_conflict_widget;

    GitGraph m_graph;

    void initializeGraph();

    void changedListSelection(const QModelIndex& index);
    void publishCurrentStep();
    void updateConflict(int row);
    void syncConflictEditors();

    void openConflictEditor(
        std::size_t id,
        const QString& path,
        const core::conflict::ConflictEntry& entry,
        ConflictWidget::ConflictEditMode mode
    );

    void editConflictResolution(
        app::conflict::ConflictFileId id, const QByteArray& content, app::conflict::ConflictResolutionType type
    );
    void showConflictEditorError(app::conflict::ConflictFileId id, QString message);
};
}
