#include "app/rebase/RebaseController.h"
#include "app/rebase/RebasePlanModel.h"
#include "core/action/Action.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/error.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebasePlan.h"

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
        core::conflict::ConflictEntry entry,
        std::optional<std::string> oldBlob,
        std::optional<std::string> newBlob
    )
        : QUndoCommand(text(oldBlob, newBlob))
        , m_controller(controller)
        , m_entry(std::move(entry))
        , m_old(std::move(oldBlob))
        , m_new(std::move(newBlob)) { }

    void redo() override { m_controller.applyResolution(m_entry, m_new); }

    void undo() override { m_controller.applyResolution(m_entry, m_old); }

private:
    static QString text(const std::optional<std::string>& oldBlob, const std::optional<std::string>& newBlob) {
        if (!newBlob.has_value()) {
            return "Reset resolution";
        }
        return oldBlob.has_value() ? "Edit resolution" : "Resolve conflict";
    }

    RebaseController& m_controller;
    core::conflict::ConflictEntry m_entry;
    std::optional<std::string> m_old;
    std::optional<std::string> m_new;
};

RebaseController::RebaseController(core::rebase::RebaseSession& session, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_analyzer(m_session.repo(), m_session.plan(), m_session.conflicts())
    , m_model(new RebasePlanModel(*this, m_session.plan(), this)) { }

RebaseController::~RebaseController() = default;

std::optional<int> RebaseController::blocking_row() const {
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

bool RebaseController::moveAction(int from, int to) {
    if (!validRow(from) || !validRow(to) || from == to) {
        return false;
    }

    m_stack.push(new MoveCommand(*this, from, to));
    return true;
}

bool RebaseController::setActionType(int row, core::action::ActionType type) {
    if (!validRow(row)) {
        return false;
    }

    const core::action::ActionType old_type = m_model->actionAt(row)->get_type();
    if (old_type == type) {
        return false;
    }

    m_stack.push(new TypeCommand(*this, row, old_type, type));
    return true;
}

bool RebaseController::setMessage(int row, const QString& message) {
    if (!validRow(row)) {
        return false;
    }

    const core::action::Action* act = m_model->actionAt(row);
    if (!act->can_edit_msg()) {
        return false;
    }

    std::optional<std::string> old_message = act->get_msg();
    std::string text                       = message.toStdString();
    if (old_message.has_value() && *old_message == text) {
        return false;
    }

    m_stack.push(new MessageCommand(*this, row, message.toStdString()));
    return true;
}

bool RebaseController::splitAction(int row, core::git::commit_t&& first, core::git::commit_t&& second) {
    if (!validRow(row) || first.get() == nullptr || second.get() == nullptr) {
        return false;
    }

    m_stack.push(new SplitCommand(*this, row, std::move(first), std::move(second)));
    return true;
}

bool RebaseController::setResolution(const core::conflict::ConflictEntry& entry, const std::string& blob_id) {
    std::optional<std::string> old_resolution;
    const std::string* current = m_session.conflicts().get_resolution(entry);
    if (current != nullptr) {
        if (*current == blob_id) {
            return false;
        }
        old_resolution = *current;
    }

    m_stack.push(new ResolutionCommand(*this, entry, std::move(old_resolution), blob_id));
    return true;
}

bool RebaseController::setResolutionDeleted(const core::conflict::ConflictEntry& entry) {
    std::optional<std::string> old_resolution;
    const std::string* current = m_session.conflicts().get_resolution(entry);
    if (current != nullptr) {
        if (current->empty()) {
            return false;
        }
        old_resolution = *current;
    }

    m_stack.push(new ResolutionCommand(*this, entry, std::move(old_resolution), ""));
    return true;
}

bool RebaseController::removeResolution(const core::conflict::ConflictEntry& entry) {
    const std::string* current = m_session.conflicts().get_resolution(entry);
    if (current == nullptr) {
        return false;
    }

    m_stack.push(new ResolutionCommand(*this, entry, *current, std::nullopt));
    return true;
}

core::Result<void>
RebaseController::setResolution(const core::conflict::ConflictEntry& entry, const QByteArray& contents) {
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

    // false means the resolution hasn't changed
    setResolution(entry, blob_id);

    return { };
}

bool RebaseController::undo() {
    if (!canUndo()) {
        return false;
    }

    m_stack.undo();
    return true;
}

bool RebaseController::redo() {
    if (!canRedo()) {
        return false;
    }

    m_stack.redo();
    return true;
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

    m_model->setAnalysis(m_analysis);
    reportNewErrors(previous.get());

    emit analysisChanged(m_analysis->first_changed);
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
