#include "core/rebase/RebasePlan.h"
#include "core/action/Action.h"
#include "core/git/parser.h"
#include "core/git/paths.h"
#include "core/git/RepoSession.h"
#include "core/git/types.h"
#include "core/Result.h"
#include <cstdint>
#include <expected>
#include <utility>

namespace core::rebase {

const action::Action* RebasePlan::action_at(std::uint32_t row) { return m_actions.get_action(row); }

RebasePlan::MoveResult RebasePlan::move_action(std::uint32_t from, std::uint32_t to) {
    if (from >= size() || to >= size()) {
        return { .changed = false, .last_unaffected = nullptr };
    }

    if (from == to) {
        return { .changed = false, .last_unaffected = nullptr };
    }

    m_revision += 1;

    action::Action* unaffected = m_actions.move(from, to);

    return { .changed = true, .last_unaffected = unaffected };
}

git::commit_t RebasePlan::split(action::Action* action, git::commit_t&& prev, git::commit_t&& next) {
    m_revision += 1;
    return m_actions.split(action, std::move(prev), std::move(next));
}

std::pair<git::commit_t, git::commit_t> RebasePlan::merge_next(action::Action* act, git::commit_t&& commit) {
    m_revision += 1;
    return m_actions.merge_next(act, std::move(commit));
}

void RebasePlan::set_type(action::Action* action, action::ActionType type) {
    m_revision += 1;
    action->set_type(type);
}

void RebasePlan::clear() {
    m_actions.clear();
    m_revision += 1;
}

void RebasePlan::clear_revisions() { m_revision = 0; }

Result<RebasePlan> RebasePlan::open(git::RepoSession& repo, git::RebaseInfo info, git_commit* root_commit) {

    const auto filepath = repo.path() + '/' + git::TODO_FILE.c_str();
    auto todo_file      = git::parse_file(filepath, repo.comment_string());

    if (todo_file.has_error()) {
        return std::unexpected<Error>({
            .title   = "Failed to parse todo file",
            .message = todo_file.err,
        });
    }

    RebasePlan plan;
    plan.m_info = info;
    plan.m_actions.set_root_commit(root_commit);

    for (const auto& raw_action : todo_file.actions) {
        switch (raw_action.type) {
        case git::CmdType::INVALID:
            std::unreachable();

        case git::CmdType::NONE:
            continue;
        case git::CmdType::PICK:
        case git::CmdType::REWORD:
        case git::CmdType::EDIT:
        case git::CmdType::SQUASH:
        case git::CmdType::FIXUP:
        case git::CmdType::DROP:
            break;

        case git::CmdType::EXEC:
        case git::CmdType::BREAK:
        case git::CmdType::LABEL:
        case git::CmdType::RESET:
        case git::CmdType::MERGE:
        case git::CmdType::UPDATE_REF:
            return std::unexpected<Error>({
                .title   = "Unsupported action in todo file",
                .message = std::format(
                    "Rebase action '{}' on line {} is not supported", git::cmd_to_str(raw_action.type), raw_action.line
                ),
            });
        }

        git_oid id;
        core::git::commit_t commit;
        if (!core::git::get_oid_from_hash(id, raw_action.hash.c_str(), repo.repo())
            || git_commit_lookup(&commit, repo.repo(), &id) != 0) {
            return std::unexpected<Error>({
                .title   = "Failed to parse todo file",
                .message = std::format("Could not find commit: {}", raw_action.hash),
            });
        }

        action::ActionType type;
        switch (raw_action.type) {
        case git::CmdType::PICK:
            type = action::ActionType::PICK;
            break;
        case git::CmdType::REWORD:
            type = action::ActionType::REWORD;
            break;
        case git::CmdType::EDIT:
            type = action::ActionType::EDIT;
            break;
        case git::CmdType::SQUASH:
            type = action::ActionType::SQUASH;
            break;
        case git::CmdType::FIXUP:
            type = action::ActionType::FIXUP;
            break;
        case git::CmdType::DROP:
            type = action::ActionType::DROP;
            break;
        case git::CmdType::INVALID:
        case git::CmdType::NONE:
        case git::CmdType::EXEC:
        case git::CmdType::BREAK:
        case git::CmdType::LABEL:
        case git::CmdType::RESET:
        case git::CmdType::MERGE:
        case git::CmdType::UPDATE_REF:
            std::unreachable();
        }

        plan.m_actions.append(action::Action { type, std::move(commit) });
    }

    return plan;
}

RebasePlan RebasePlan::open(git::RebaseInfo info, action::ActionsManager&& actions) {
    return { std::move(info), std::move(actions) };
}

}
