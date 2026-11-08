#include "app/rebase/RebaseController.h"
#include "app/conflict/ConflictEditorSession.h"
#include "app/rebase/RebasePlanModel.h"
#include "core/action/Action.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/error.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebasePlan.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <git2/types.h>

#include <QObject>
#include <Qt>
#include <QUndoCommand>

namespace app::rebase {

class RebaseController::MoveCommand : public QUndoCommand {
public:
    MoveCommand(RebaseController& controller, int from, int to)
        : QUndoCommand("Move action")
        , m_controller(controller)
        , m_from(from)
        , m_to(to) { }

    void redo() override { m_controller.applyMove(m_from, m_to); }

    void undo() override { m_controller.applyMove(m_to, m_from); }

private:
    RebaseController& m_controller;
    int m_from;
    int m_to;
};

class RebaseController::TypeCommand : public QUndoCommand {
public:
    TypeCommand(
        RebaseController& controller, int row, core::action::ActionType oldType, core::action::ActionType newType
    )
        : QUndoCommand(QStringLiteral("Change action to %1").arg(core::action::type_to_str(newType)))
        , m_controller(controller)
        , m_row(row)
        , m_old(oldType)
        , m_new(newType) { }

    void redo() override { m_controller.applyType(m_row, m_new); }

    void undo() override { m_controller.applyType(m_row, m_old); }

private:
    RebaseController& m_controller;
    int m_row;
    core::action::ActionType m_old;
    core::action::ActionType m_new;
};

class RebaseController::MessageCommand : public QUndoCommand {
public:
    static constexpr int command_id = 1;

    MessageCommand(RebaseController& controller, int row, std::string message)
        : QUndoCommand("Edit commit message")
        , m_controller(controller)
        , m_row(row)
        , m_new(std::move(message)) { }

    void redo() override {
        auto previous = m_controller.applyMessage(m_row, m_new);

        if (!m_captured) {
            m_old      = std::move(previous);
            m_captured = true;
        }
    }

    void undo() override { m_controller.applyMessage(m_row, m_old); }

    [[nodiscard]] int id() const override { return command_id; }

    // merge message commands
    bool mergeWith(const QUndoCommand* other) override {
        const auto* next = static_cast<const MessageCommand*>(other);
        if (next->m_row != m_row) {
            return false;
        }

        m_new = next->m_new;
        return true;
    }

private:
    RebaseController& m_controller;
    int m_row;
    std::optional<std::string> m_old;
    std::optional<std::string> m_new;
    bool m_captured = false;
};

class RebaseController::SplitCommand final : public QUndoCommand {
public:
    SplitCommand(RebaseController& controller, int row, core::git::commit_t&& first, core::git::commit_t&& second)
        : QUndoCommand("Split commit")
        , m_controller(controller)
        , m_row(row)
        , m_first(std::move(first))
        , m_second(std::move(second)) { }

    void redo() override { m_original = m_controller.applySplit(m_row, std::move(m_first), std::move(m_second)); }

    void undo() override {
        auto commits = m_controller.applyMerge(m_row, std::move(m_original));

        m_first  = std::move(commits.first);
        m_second = std::move(commits.second);
    }

private:
    RebaseController& m_controller;
    int m_row;

    core::git::commit_t m_first;
    core::git::commit_t m_second;

    core::git::commit_t m_original;
};

class RebaseController::ResolutionCommand : public QUndoCommand {
public:
    ResolutionCommand(
        RebaseController& controller,
        conflict::ConflictFileId id,
        std::optional<std::string> oldBlob,
        std::optional<std::string> newBlob
    )
        : QUndoCommand(text(oldBlob, newBlob))
        , m_controller(controller)
        , m_id(std::move(id))
        , m_old(std::move(oldBlob))
        , m_new(std::move(newBlob)) { }

    [[nodiscard]] const conflict::ConflictFileId& file() const { return m_id; }

    [[nodiscard]] bool setsResolution() const { return m_new.has_value(); }

    void redo() override { m_controller.applyResolution(m_id.entry, m_new); }

    void undo() override { m_controller.applyResolution(m_id.entry, m_old); }

private:
    static QString text(const std::optional<std::string>& oldBlob, const std::optional<std::string>& newBlob) {
        if (!newBlob.has_value()) {
            return "Reset resolution";
        }
        return oldBlob.has_value() ? "Edit resolution" : "Resolve conflict";
    }

    RebaseController& m_controller;
    conflict::ConflictFileId m_id;
    std::optional<std::string> m_old;
    std::optional<std::string> m_new;
};

RebaseController::RebaseController(
    core::rebase::RebaseSession& session, const conflict::ConflictEditor& editor, QObject* parent
)
    : QObject(parent)
    , m_session(session)
    , m_analyzer(m_session.repo(), m_session.plan(), m_session.conflicts())
    , m_model(new RebasePlanModel(*this, m_session.plan(), this))
    , m_editors(new conflict::ConflictEditorManager(editor, this)) { }

RebaseController::~RebaseController() = default;

std::optional<int> RebaseController::blockingRow() const {
    if (m_analysis == nullptr) {
        return std::nullopt;
    }

    const StepResult* step = m_analysis->blocking();
    if (step == nullptr) {
        return std::nullopt;
    }

    return static_cast<int>(step->index);
}

bool RebaseController::validRow(int row) const { return row >= 0 && row < m_model->rowCount(); }

bool RebaseController::runGuarded(GuardedAction action, GateScope scope) {
    bool blocked = false;

    switch (scope.kind) {
    case GateScope::Kind::None:
        blocked = false;
        break;
    case GateScope::Kind::Global:
        blocked = m_editors->hasOpenEditors();
        break;
    case GateScope::Kind::File:
        blocked = m_editors->isOpen(*scope.file);
        break;
    }

    if (!blocked) {
        return action();
    }

    m_pending.action = std::move(action);
    m_pending.scope  = scope;

    emit blockedByOpenEditors(
        m_pending.scope.kind == GateScope::Kind::File ? QList { *m_pending.scope.file } : m_editors->openIds()
    );
    return false;
}

bool RebaseController::completesConflict(const conflict::ConflictFileId& id) const {
    if (m_analysis == nullptr) {
        return false;
    }

    const auto* block = m_analysis->blocking();
    if (block == nullptr || block->conflict == nullptr) {
        return false;
    }

    return std::ranges::all_of(block->conflict->files, [&](const core::rebase::ConflictFile& f) -> bool {
        if (f.path == id.path && f.entry == id.entry) {
            return true;
        }

        return f.resolved() || m_session.conflicts().get_resolution(f.entry) != nullptr;
    });
}

RebaseController::GateScope RebaseController::scopeOf(const QUndoCommand* cmd, bool is_undo) const {
    const auto* resolution = dynamic_cast<const ResolutionCommand*>(cmd);
    if (resolution != nullptr) {
        const auto file = resolution->file();
        if (is_undo) {
            // current conflict resolution
            if (m_conflict_files.contains(file)) {
                return GateScope::forFile(file);
            }
            // previous conflict resolution
            else {
                return GateScope::global();
            }
        }

        if (resolution->setsResolution() && completesConflict(file)) {
            return GateScope::global();
        } else {
            return GateScope::forFile(file);
        }
    }

    if (dynamic_cast<const MessageCommand*>(cmd) != nullptr) {
        return GateScope::none();
    }

    return GateScope::global();
}

void RebaseController::discardEditorsAndContinue() {
    if (m_pending.scope.kind == GateScope::Kind::File) {
        m_editors->discardOne(*m_pending.scope.file);
    } else {
        m_editors->discardAll();
    }

    runPending();
}

void RebaseController::cancelPending() {
    m_pending.action = nullptr;
    m_pending.scope  = GateScope::global();
}

void RebaseController::runPending() {
    auto action     = std::exchange(m_pending.action, nullptr);
    m_pending.scope = GateScope::global();

    if (action != nullptr) {
        action();
    }
}

bool RebaseController::undo() {
    if (!canUndo()) {
        return false;
    }

    const auto* cmd = m_stack.command(m_stack.index() - 1);
    return runGuarded(
        [this]() -> bool {
            // re-validate after the signal
            if (!canUndo()) {
                return false;
            }

            m_stack.undo();
            return true;
        },
        scopeOf(cmd, /*is_undo=*/true)
    );
}

bool RebaseController::redo() {
    if (!canRedo()) {
        return false;
    }

    const auto* cmd = m_stack.command(m_stack.index());
    return runGuarded(
        [this]() -> bool {
            // re-validate after the signal
            if (!canRedo()) {
                return false;
            }

            m_stack.redo();
            return true;
        },
        scopeOf(cmd, /*is_undo=*/false)
    );
}

bool RebaseController::canMove(int from, int to) const { return validRow(from) && validRow(to) && from != to; }

bool RebaseController::moveAction(int from, int to) {
    if (!canMove(from, to)) {
        return false;
    }

    return runGuarded([this, from, to]() -> bool {
        // re-validate after the signal
        if (!canMove(from, to)) {
            return false;
        }

        m_stack.push(new MoveCommand(*this, from, to));
        return true;
    });
}

bool RebaseController::willChangeConflict(core::action::ActionType old_type, core::action::ActionType new_type) const {
    if (old_type == new_type) {
        return false;
    }

    // drop -> x or x -> drop
    return old_type == core::action::ActionType::DROP || new_type == core::action::ActionType::DROP;
}

bool RebaseController::setActionType(int row, core::action::ActionType type) {
    if (!validRow(row)) {
        return false;
    }

    const core::action::ActionType old_type = m_model->actionAt(row)->get_type();
    if (old_type == type) {
        return false;
    }

    GateScope scope;
    if (willChangeConflict(old_type, type)) {
        scope = GateScope::global();
    } else {
        scope = GateScope::none();
    }

    return runGuarded(
        [this, row, type]() -> bool {
            // re-validate after the signal
            if (!validRow(row)) {
                return false;
            }

            const core::action::ActionType old = m_model->actionAt(row)->get_type();
            if (old == type) {
                return false;
            }

            m_stack.push(new TypeCommand(*this, row, old, type));
            return true;
        },
        scope
    );
}

bool RebaseController::canSetMessage(int row, const QString& message) const {
    if (!validRow(row)) {
        return false;
    }

    const core::action::Action* act = m_model->actionAt(row);
    if (!act->can_edit_msg()) {
        return false;
    }

    std::optional<std::string> old_message = act->get_msg();
    return !old_message.has_value() || *old_message != message;
}

bool RebaseController::setMessage(int row, const QString& message) {
    if (!canSetMessage(row, message)) {
        return false;
    }

    return runGuarded(
        [this, row, message]() -> bool {
            if (!canSetMessage(row, message)) {
                return false;
            }
            m_stack.push(new MessageCommand(*this, row, message.toStdString()));
            return true;
        },
        GateScope::none()
    );
}

bool RebaseController::splitAction(int row, core::git::commit_t&& first, core::git::commit_t&& second) {
    if (!validRow(row) || first.get() == nullptr || second.get() == nullptr) {
        return false;
    }

    return runGuarded([this, row, f = std::move(first), s = std::move(second)]() mutable -> bool {
        if (!validRow(row) || f.get() == nullptr || s.get() == nullptr) {
            return false;
        }

        m_stack.push(new SplitCommand(*this, row, std::move(f), std::move(s)));
        return true;
    });
}

bool RebaseController::commitResolution(const conflict::ConflictFileId& id, std::optional<std::string> blob) {
    std::optional<std::string> old_resolution;
    const std::string* current = m_session.conflicts().get_resolution(id.entry);
    if (current != nullptr) {
        if (blob.has_value() && *current == *blob) {
            return false;
        }

        old_resolution = *current;

    } else if (current == nullptr && !blob.has_value()) {
        return false;
    }

    const auto before = blockingRow();
    m_stack.push(new ResolutionCommand(*this, id, std::move(old_resolution), std::move(blob)));

    // conflict advanced
    if (blockingRow() != before) {
        m_editors->discardExcept(currentConflictFiles());
    }

    return true;
}

bool RebaseController::setResolution(const conflict::ConflictFileId& id, const std::string& blob_id) {
    return runGuarded([this, id, blob_id]() -> bool { return commitResolution(id, blob_id); }, GateScope::forFile(id));
}

bool RebaseController::setResolutionDeleted(const conflict::ConflictFileId& id) {
    return runGuarded([this, id]() -> bool { return commitResolution(id, ""); }, GateScope::forFile(id));
}

bool RebaseController::removeResolution(const conflict::ConflictFileId& id) {
    return runGuarded([this, id]() -> bool { return commitResolution(id, std::nullopt); }, GateScope::forFile(id));
}

void RebaseController::setEditorResolution(const conflict::ConflictFileId& id) { commitResolution(id, ""); }

core::Result<void>
RebaseController::setEditorResolution(const conflict::ConflictFileId& id, const QByteArray& contents) {
    git_oid oid { };

    if (git_blob_create_from_buffer(
            &oid, m_session.repo().repo(), contents.constData(), static_cast<std::size_t>(contents.size())
        )
        != 0) {
        return std::unexpected<core::Error>({
            .title   = "Resolution error",
            .message = core::git::get_last_error(),
        });
    }

    std::string blob_id = core::git::format_oid_to_str<core::git::OID_SIZE>(&oid);

    // does not run inside the guard
    commitResolution(id, blob_id);

    return { };
}

void RebaseController::markSaved() {
    m_stack.setClean();
    afterChange();
}

void RebaseController::afterChange() { refresh(false); }

void RebaseController::refresh(bool force) {
    if (!force && m_analysis != nullptr && m_analyzer.is_current(*m_analysis)) {
        return;
    }

    auto result = m_analyzer.analyze(force ? nullptr : m_analysis.get());
    if (!result.has_value()) {
        emit errorOccurred(
            QString::fromStdString(result.error().title), QString::fromStdString(result.error().message)
        );
        return;
    }

    AnalysisPtr previous = std::move(m_analysis);
    m_analysis           = result.value();
    m_conflict_files     = buildConflictFiles();

    m_model->setAnalysis(m_analysis);
    reportNewErrors(previous.get());

    emit analysisChanged(m_analysis->first_changed);
}

QSet<conflict::ConflictFileId> RebaseController::buildConflictFiles() const {
    QSet<conflict::ConflictFileId> ids;
    if (!m_analysis) {
        return ids;
    }

    const auto* block = m_analysis->blocking();
    if (block == nullptr || block->conflict == nullptr) {
        return ids;
    }

    const auto& files = block->conflict->files;
    ids.reserve(static_cast<int>(files.size()));

    for (const auto& f : files) {
        ids.insert({ .path = f.path, .entry = f.entry });
    }

    return ids;
}

void RebaseController::reportNewErrors(const core::rebase::RebaseAnalysis* previous) {
    for (const StepResult& step : m_analysis->steps) {
        if (step.status != core::conflict::ConflictState::Error || !step.error.has_value()) {
            continue;
        }

        if (previous != nullptr) {
            const StepResult* old = previous->find(step.input.commit);

            const bool has_old_err
                = old != nullptr && old->status == core::conflict::ConflictState::Error && old->error.has_value();

            // same error
            if (has_old_err && old->error->message == step.error->message) {
                continue;
            }
        }

        emit errorOccurred(QString::fromStdString(step.error->title), QString::fromStdString(step.error->message));
    }
}

void RebaseController::applyMove(int from, int to) {
    m_model->beginMove(from, to);
    m_session.plan().move_action(static_cast<std::uint32_t>(from), static_cast<std::uint32_t>(to));
    m_model->endMove();

    afterChange();
}

void RebaseController::applyType(int row, core::action::ActionType type) {

    m_session.plan().set_type(m_model->actionAt(row), type);

    m_model->rowChanged(
        row, { RebasePlanModel::TypeRole, RebasePlanModel::MessageRole, RebasePlanModel::CanEditMessageRole }
    );

    afterChange();
}

std::optional<std::string> RebaseController::applyMessage(int row, const std::optional<std::string>& message) {
    core::action::Action* act = m_model->actionAt(row);

    std::optional<std::string> previous = act->get_msg();

    if (!message.has_value()) {
        act->clear_msg();
    } else {
        act->set_msg(*message);
    }

    m_model->rowChanged(row, { RebasePlanModel::MessageRole });
    afterChange();

    return previous;
}

core::git::commit_t RebaseController::applySplit(int row, core::git::commit_t&& first, core::git::commit_t&& second) {
    core::action::Action* act = m_model->actionAt(row);
    core::git::commit_t original;

    m_model->replaceRows(row, /*removed=*/1, /*inserted=*/2, [&] {
        original = m_session.plan().split(act, std::move(first), std::move(second));
    });

    afterChange();

    return original;
}

std::pair<core::git::commit_t, core::git::commit_t>
RebaseController::applyMerge(int row, core::git::commit_t&& commit) {
    core::action::Action* act = m_model->actionAt(row);
    std::pair<core::git::commit_t, core::git::commit_t> commits;

    m_model->replaceRows(row, /*removed=*/2, /*inserted=*/1, [&] {
        commits = m_session.plan().merge_next(act, std::move(commit));
    });

    afterChange();

    return commits;
}

void RebaseController::applyResolution(
    const core::conflict::ConflictEntry& entry, const std::optional<std::string>& blob_id
) {
    auto& conflicts = m_session.conflicts();
    if (blob_id.has_value()) {
        conflicts.add_resolution(entry, *blob_id);
    } else {
        conflicts.remove_resolution(entry);
    }

    afterChange();
}
}
