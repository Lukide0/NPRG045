#pragma once

#include "app/conflict/ConflictEditorManager.h"
#include "app/conflict/ConflictEditorSession.h"
#include "app/rebase/RebasePlanModel.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebaseAnalyzer.h"

#include "core/action/Action.h"
#include "core/rebase/RebaseSession.h"
#include <cstddef>
#include <functional>
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
    using AnalysisPtr   = core::rebase::RebaseAnalysisPtr;
    using StepResult    = core::rebase::StepResult;
    using GuardedAction = std::move_only_function<bool()>;

    struct GateScope {
        enum class Kind {
            None,
            Global,
            File,
        } kind;
        std::optional<conflict::ConflictFileId> file;

        static GateScope none() { return { .kind = Kind::None, .file = std::nullopt }; }

        static GateScope global() { return { .kind = Kind::Global, .file = std::nullopt }; }

        static GateScope forFile(conflict::ConflictFileId id) { return { .kind = Kind::File, .file = std::move(id) }; }
    };

    RebaseController(
        core::rebase::RebaseSession& session, const conflict::ConflictEditor& editor, QObject* parent = nullptr
    );

    ~RebaseController() override;

    RebaseController(const RebaseController&)            = delete;
    RebaseController& operator=(const RebaseController&) = delete;

    [[nodiscard]] RebasePlanModel* model() const { return m_model; }

    [[nodiscard]] core::rebase::RebaseSession& session() const { return m_session; }

    [[nodiscard]] conflict::ConflictEditorManager* editors() const { return m_editors; }

    /** @brief The latest analysis. Never null after construction unless the first analysis failed. */
    [[nodiscard]] AnalysisPtr analysis() const { return m_analysis; }

    [[nodiscard]] QUndoStack* undoStack() { return &m_stack; }

    /** @brief Row of the unresolved conflict that blocks the analysis, if any. */
    [[nodiscard]] std::optional<int> blockingRow() const;

    /** @brief Moves the action at 'from' so that it ends up at row 'to'. */
    bool moveAction(int from, int to);

    /** @brief Sets the action type. */
    bool setActionType(int row, core::action::ActionType type);

    /** @brief Sets the message of a reword action. False for other action types. */
    bool setMessage(int row, const QString& message);

    /** @brief Replaces the action by two: 'first' stays at 'row', 'second' is inserted after it. */
    bool splitAction(int row, core::git::commit_t&& first, core::git::commit_t&& second);

    /** @brief Records or overwrites the resolution (a blob id; empty means the file is deleted). */
    bool setResolution(const conflict::ConflictFileId& id, const std::string& blob_id);

    /** @brief Records or overwrites the resolution. */
    bool setResolutionDeleted(const conflict::ConflictFileId& id);

    /** @brief Removes the recorded resolution, so the conflict is unresolved again. */
    bool removeResolution(const conflict::ConflictFileId& id);

    /** @brief Records or overwrites the resolution. Does not run inside the guard. */
    core::Result<void> setEditorResolution(const conflict::ConflictFileId& id, const QByteArray& contents);

    /** @brief Records or overwrites the resolution. Does not run inside the guard. */
    void setEditorResolution(const conflict::ConflictFileId& id);

    bool undo();
    bool redo();

    [[nodiscard]] bool canUndo() const { return m_stack.canUndo(); }

    [[nodiscard]] bool canRedo() const { return m_stack.canRedo(); }

    /** @brief True if there are changes since the last mark_saved(). */
    [[nodiscard]] bool modified() const { return !m_stack.isClean(); }

    void markSaved();

    /**
     * @brief Analyzes again. 'force' ignores the previous analysis. Use it after the plan or the resolutions were
     * changed without the controller. */
    void refresh(bool force = false);

    [[nodiscard]] const QSet<conflict::ConflictFileId>& currentConflictFiles() const { return m_conflict_files; }

    /** @brief Discards all active conflict editors and executes pending action */
    void discardEditorsAndContinue();
    /** @brief Discards pending action */
    void cancelPending();

signals:
    /** @brief A new analysis is available (and the model was updated). */
    void analysisChanged(std::size_t first_changed_row);

    /** @brief Something failed. Emitted for a failed analysis and for each step that newly fails. */
    void errorOccurred(const QString& title, const QString& message);

    /** @brief Could not execute command because it would modify open editor */
    void blockedByOpenEditors(QList<conflict::ConflictFileId> editors);

private:
    class MoveCommand;
    class TypeCommand;
    class MessageCommand;
    class SplitCommand;
    class ResolutionCommand;

    core::rebase::RebaseSession& m_session;
    core::rebase::RebaseAnalyzer m_analyzer;
    AnalysisPtr m_analysis;
    QSet<conflict::ConflictFileId> m_conflict_files;

    QUndoStack m_stack;

    struct {
        GuardedAction action;
        GateScope scope;
    } m_pending;

    RebasePlanModel* m_model;
    conflict::ConflictEditorManager* m_editors;

    bool commitResolution(const conflict::ConflictFileId& id, std::optional<std::string> blob);

    void applyMove(int from, int to);
    void applyType(int row, core::action::ActionType type);
    std::optional<std::string> applyMessage(int row, const std::optional<std::string>& message);
    core::git::commit_t applySplit(int row, core::git::commit_t&& first, core::git::commit_t&& second);
    std::pair<core::git::commit_t, core::git::commit_t> applyMerge(int row, core::git::commit_t&& commit);
    void applyResolution(const core::conflict::ConflictEntry& entry, const std::optional<std::string>& blob_id);

    void afterChange();
    void reportNewErrors(const core::rebase::RebaseAnalysis* previous);

    [[nodiscard]] QSet<conflict::ConflictFileId> buildConflictFiles() const;

    [[nodiscard]] bool validRow(int row) const;

    bool runGuarded(GuardedAction action, GateScope scope = GateScope::global());
    GateScope scopeOf(const QUndoCommand* cmd, bool is_undo) const;
    void runPending();

    // checks

    [[nodiscard]] bool canMove(int from, int to) const;
    [[nodiscard]] bool willChangeConflict(core::action::ActionType old_type, core::action::ActionType new_type) const;
    [[nodiscard]] bool canSetMessage(int row, const QString& message) const;
    [[nodiscard]] bool completesConflict(const conflict::ConflictFileId& id) const;
};

}
