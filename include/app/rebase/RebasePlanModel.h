#pragma once

#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebasePlan.h"

#include <functional>

#include <git2/types.h>
#include <QAbstractListModel>
#include <QObject>
#include <Qt>
#include <vector>

namespace app::rebase {

class RebaseController;

class RebasePlanModel : public QAbstractListModel {
    Q_OBJECT

public:
    using Action           = core::action::Action;
    using StepResult       = core::rebase::StepResult;
    using AnalysisPtr      = core::rebase::RebaseAnalysisPtr;
    using AnalysisConstPtr = core::rebase::RebaseAnalysisConstPtr;

    enum Role {
        HashRole = Qt::UserRole + 1, // short hash (QString)
        CommitIdRole, // full commit id (QString)
        SummaryRole, // first line of the commit message (QString)
        TypeRole, // ActionType (int), editable
        ConflictStateRole, // ConflictState (int)
        ConflictFileCountRole, // conflicted files of this step (int)
        UnresolvedCountRole, // files without a resolution (int)
        ErrorRole, // error text of a failed step (QString)
        MessageRole, // message of a reword action (QString), editable
        CanEditMessageRole, // bool
        AffectsLaterConflictRole, // bool
    };

    RebasePlanModel(RebaseController& controller, core::rebase::RebasePlan& plan, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = { }) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    bool moveRows(
        const QModelIndex& source_parent,
        int source_row,
        int count,
        const QModelIndex& destination_parent,
        int destination_child
    ) override;

    [[nodiscard]] Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }

    [[nodiscard]] Action* actionAt(int row) const;
    [[nodiscard]] int rowOf(const Action* act) const;
    [[nodiscard]] const StepResult* stepAt(int row) const;

private:
    friend class RebaseController;

    void beginMove(int from, int to);
    void endMove();

    void replaceRows(int row, int removed, int inserted, const std::function<void()>& mutate);
    void rowChanged(int row, const QList<int>& roles);
    void setAnalysis(AnalysisPtr analysis);

    void rebuildRows();
    void rebuildSteps();

    RebaseController& m_controller;
    core::rebase::RebasePlan& m_plan;

    AnalysisPtr m_analysis;
    std::vector<Action*> m_rows;
    std::vector<const StepResult*> m_steps;
};

}
