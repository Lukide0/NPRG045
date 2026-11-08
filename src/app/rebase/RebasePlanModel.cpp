#include "app/rebase/RebasePlanModel.h"
#include "app/rebase/RebaseController.h"
#include "core/action/Action.h"
#include "core/conflict/conflict.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebasePlan.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <git2/commit.h>
#include <git2/oid.h>
#include <QAbstractListModel>
#include <QObject>
#include <QStringLiteral>
#include <Qt>
#include <utility>

namespace app::rebase {

QString summary_of(core::action::Action& act) {
    const char* summary = git_commit_summary(act.get_commit());
    return summary != nullptr ? QString::fromUtf8(summary) : QString();
}

QString short_hash_of(core::action::Action& act) {
    return QString::fromStdString(core::git::format_oid_to_str(&act.get_oid()));
}

QString full_hash_of(core::action::Action& act) {
    return QString::fromStdString(core::git::format_oid_to_str<core::git::OID_SIZE>(&act.get_oid()));
}

RebasePlanModel::RebasePlanModel(RebaseController& controller, core::rebase::RebasePlan& plan, QObject* parent)
    : QAbstractListModel(parent)
    , m_controller(controller)
    , m_plan(plan) {
    rebuildRows();
}

int RebasePlanModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant RebasePlanModel::data(const QModelIndex& index, int role) const {
    using core::conflict::ConflictState;

    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return { };
    }

    const auto row   = static_cast<std::size_t>(index.row());
    Action& act      = *m_rows[row];
    const auto* step = m_steps[row];

    if (step == nullptr) {
        return { };
    }

    const int conflict_files   = step->conflict == nullptr ? 0 : static_cast<int>(step->conflict->files.size());
    const int unresolved_files = step->conflict == nullptr ? 0 : static_cast<int>(step->conflict->unresolved_count());
    const QString error        = step->error.has_value() ? QString::fromStdString(step->error->message) : QString();

    switch (role) {
    case Qt::DisplayRole:
    case SummaryRole:
        return summary_of(act);

    case HashRole:
        return short_hash_of(act);

    case CommitIdRole:
        return full_hash_of(act);

    case TypeRole:
        return static_cast<int>(act.get_type());

    case ConflictStateRole:
        return static_cast<int>(step->status);

    case ConflictFileCountRole:
        return conflict_files;

    case UnresolvedCountRole:
        return unresolved_files;

    case ErrorRole:
        return error;

    case MessageRole: {
        if (act.has_msg()) {
            return QString::fromStdString(*act.get_msg());
        }
        return QString();
    }

    case CanEditMessageRole:
        return act.can_edit_msg();

    case AffectsLaterConflictRole:
        return step->extra.affects_later_conflict;

    case Qt::ToolTipRole: {
        QString text = short_hash_of(act) + ' ' + summary_of(act);

        switch (step->status) {
        case ConflictState::Conflict:
            text += '\n'
                + QStringLiteral("Conflict in %1 file(s), %2 without a resolution")
                      .arg(conflict_files)
                      .arg(unresolved_files);
            break;
        case ConflictState::Resolved:
            text += '\n' + QStringLiteral("Conflict resolved in %1 file(s)").arg(conflict_files);
            break;
        case ConflictState::Error:
            text += '\n' + error;
            break;
        case ConflictState::Unknown:
        case ConflictState::Clean:
            break;
        }

        return text;
    }

    default:
        return { };
    }
}

bool RebasePlanModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return false;
    }

    switch (role) {
    case TypeRole: {
        bool ok       = false;
        const int raw = value.toInt(&ok);

        if (!ok || raw < 0 || !core::action::is_valid_type(static_cast<core::action::ActionType>(raw))) {
            return false;
        }

        return m_controller.setActionType(index.row(), static_cast<core::action::ActionType>(raw));
    }

    case MessageRole:
        return m_controller.setMessage(index.row(), value.toString());

    default:
        return false;
    }
}

Qt::ItemFlags RebasePlanModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::ItemIsDropEnabled;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
}

QHash<int, QByteArray> RebasePlanModel::roleNames() const {
    auto roles = QAbstractListModel::roleNames();

    roles[HashRole]                 = "hash";
    roles[CommitIdRole]             = "commitId";
    roles[SummaryRole]              = "summary";
    roles[TypeRole]                 = "type";
    roles[ConflictStateRole]        = "conflictState";
    roles[ConflictFileCountRole]    = "conflictFileCount";
    roles[UnresolvedCountRole]      = "unresolvedCount";
    roles[ErrorRole]                = "error";
    roles[MessageRole]              = "message";
    roles[CanEditMessageRole]       = "canEditMessage";
    roles[AffectsLaterConflictRole] = "affectsLaterConflict";

    return roles;
}

bool RebasePlanModel::moveRows(
    const QModelIndex& source_parent,
    int source_row,
    int count,
    const QModelIndex& destination_parent,
    int destination_child
) {
    if (source_parent.isValid() || destination_parent.isValid() || count != 1) {
        return false;
    }

    if (source_row < 0 || source_row >= rowCount() || destination_child < 0 || destination_child > rowCount()) {
        return false;
    }

    // Qt gives the row to insert before; the controller wants the final row
    const int to = destination_child > source_row ? destination_child - 1 : destination_child;
    if (to == source_row) {
        return false;
    }

    // the controller changes the plan and calls beginMove()/endMove(), which emit the signals
    return m_controller.moveAction(source_row, to);
}

core::action::Action* RebasePlanModel::actionAt(int row) const {
    if (row < 0 || row >= rowCount()) {
        return nullptr;
    }
    return m_rows[static_cast<std::size_t>(row)];
}

int RebasePlanModel::rowOf(const Action* act) const {
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i] == act) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const core::rebase::StepResult* RebasePlanModel::stepAt(int row) const {
    if (row < 0 || row >= rowCount()) {
        return nullptr;
    }
    return m_steps[static_cast<std::size_t>(row)];
}

void RebasePlanModel::beginMove(int from, int to) {
    beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to);
}

void RebasePlanModel::endMove() {
    rebuildRows();
    endMoveRows();
}

void RebasePlanModel::replaceRows(int row, int removed, int inserted, const std::function<void()>& mutate) {
    const auto first = static_cast<std::ptrdiff_t>(row);
    const auto last  = first + removed;
    {
        beginRemoveRows(QModelIndex(), row, row + removed - 1);

        mutate();

        m_rows.erase(m_rows.begin() + first, m_rows.begin() + last);
        m_steps.erase(m_steps.begin() + first, m_steps.begin() + last);

        endRemoveRows();
    }
    {
        beginInsertRows(QModelIndex(), row, row + inserted - 1);

        rebuildRows();

        endInsertRows();
    }
}

void RebasePlanModel::rowChanged(int row, const QList<int>& roles) {
    if (row < 0 || row >= rowCount()) {
        return;
    }

    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, roles);
}

void RebasePlanModel::setAnalysis(AnalysisPtr analysis) {

    const auto old_analysis = m_analysis;
    const auto old_steps    = m_steps;

    m_analysis = std::move(analysis);
    rebuildSteps();

    const QList<int> roles {
        ConflictStateRole, ConflictFileCountRole,    UnresolvedCountRole,
        ErrorRole,         AffectsLaterConflictRole, Qt::ToolTipRole,
    };

    auto same = [&](std::size_t i) {
        if (i >= old_steps.size()) {
            return false;
        }

        if (old_steps[i] == nullptr || m_steps[i] == nullptr) {
            return old_steps[i] == m_steps[i];
        }

        return old_steps[i]->equivalent(*m_steps[i]);
    };

    for (std::size_t i = 0; i < m_steps.size();) {
        if (same(i)) {
            ++i;
            continue;
        }

        const std::size_t first = i;

        for (; i < m_steps.size() && !same(i); ++i) { }

        emit dataChanged(index(static_cast<int>(first)), index(static_cast<int>(i - 1)), roles);
    }
}

void RebasePlanModel::rebuildRows() {
    m_rows.clear();

    for (Action& act : m_plan) {
        m_rows.push_back(&act);
    }

    rebuildSteps();
}

void RebasePlanModel::rebuildSteps() {
    using core::conflict::ConflictState;

    m_steps.assign(m_rows.size(), nullptr);
    if (m_analysis == nullptr) {
        return;
    }

    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        const git_oid& oid = m_rows[i]->get_oid();

        const StepResult* step = nullptr;
        if (i < m_analysis->steps.size() && git_oid_equal(&m_analysis->steps[i].input.commit, &oid) != 0) {
            step = &m_analysis->steps[i];
        } else {
            step = m_analysis->find(oid);
        }

        m_steps[i] = step;
    }
}
}
