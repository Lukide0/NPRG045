#pragma once

#include "app/rebase/RebasePlanModel.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/GitGraph.h"
#include "core/git/RepoSession.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebaseAnalyzer.h"

#include "core/action/Action.h"
#include "core/rebase/RebasePlan.h"
#include "core/rebase/RebaseSession.h"
#include <cstddef>
#include <git2/types.h>

#include <optional>
#include <QObject>
#include <QUndoStack>
#include <string>
#include <utility>

namespace app::rebase {

class RebaseController : public QObject {
    Q_OBJECT

public:
    using AnalysisPtr = core::rebase::RebaseAnalysisPtr;
    using StepResult  = core::rebase::StepResult;

    RebaseController(core::rebase::RebaseSession& session, QObject* parent = nullptr);

    ~RebaseController() override;

    RebaseController(const RebaseController&)            = delete;
    RebaseController& operator=(const RebaseController&) = delete;

    [[nodiscard]] RebasePlanModel* model() const { return m_model; }

    [[nodiscard]] core::rebase::RebaseSession& session() const { return m_session; }

    /// The latest analysis. Never null after construction unless the first analysis failed.
    [[nodiscard]] AnalysisPtr analysis() const { return m_analysis; }

    [[nodiscard]] QUndoStack* undoStack() { return &m_stack; }

    /// Row of the unresolved conflict that blocks the analysis, if any.
    [[nodiscard]] std::optional<int> blocking_row() const;

    /// Moves the action at 'from' so that it ends up at row 'to'.
    bool moveAction(int from, int to);

    bool setActionType(int row, core::action::ActionType type);

    /// Sets the message of a reword action. False for other action types.
    bool setMessage(int row, const QString& message);

    /// Replaces the action by two: 'first' stays at 'row', 'second' is inserted after it.
    bool splitAction(int row, core::git::commit_t&& first, core::git::commit_t&& second);

    /// Records or overwrites the resolution (a blob id; empty means the file is deleted).
    bool setResolution(const core::conflict::ConflictEntry& entry, const std::string& blob_id);

    /// Records or overwrites the resolution.
    bool setResolutionDeleted(const core::conflict::ConflictEntry& entry);

    /// Removes the recorded resolution, so the conflict is unresolved again.
    bool removeResolution(const core::conflict::ConflictEntry& entry);

    /// Records or overwrites the resolution.
    core::Result<void> setResolution(const core::conflict::ConflictEntry& entry, const QByteArray& contents);

    bool undo();
    bool redo();

    [[nodiscard]] bool canUndo() const { return m_stack.canUndo(); }

    [[nodiscard]] bool canRedo() const { return m_stack.canRedo(); }

    /// True if there are changes since the last mark_saved().
    [[nodiscard]] bool modified() const { return !m_stack.isClean(); }

    void markSaved();

    /// Analyzes again. 'force' ignores the previous analysis. Use it after the plan or the
    /// resolutions were changed without the controller.
    void refresh(bool force = false);

signals:
    /// A new analysis is available (and the model was updated).
    void analysisChanged(std::size_t first_changed_row);

    /// Something failed. Emitted for a failed analysis and for each step that newly fails.
    void errorOccurred(const QString& title, const QString& message);

private:
    core::rebase::RebaseSession& m_session;
    core::rebase::RebaseAnalyzer m_analyzer;
    AnalysisPtr m_analysis;

    QUndoStack m_stack;
    RebasePlanModel* m_model;

    void applyMove(int from, int to);
    void applyType(int row, core::action::ActionType type);
    std::optional<std::string> applyMessage(int row, const std::optional<std::string>& message);
    core::git::commit_t applySplit(int row, core::git::commit_t&& first, core::git::commit_t&& second);
    std::pair<core::git::commit_t, core::git::commit_t> applyMerge(int row, core::git::commit_t&& commit);
    void applyResolution(const core::conflict::ConflictEntry& entry, const std::optional<std::string>& blob_id);

    void afterChange();
    void reportNewErrors(const core::rebase::RebaseAnalysis* previous);

    [[nodiscard]] bool validRow(int row) const;

    class MoveCommand;
    class TypeCommand;
    class MessageCommand;
    class SplitCommand;
    class ResolutionCommand;
};

}
