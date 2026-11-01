#pragma once

#include "core/action/Action.h"
#include "core/action/ActionsManager.h"
#include "core/git/RepoSession.h"
#include "core/git/types.h"
#include "core/Result.h"
#include <cstddef>
#include <cstdint>
#include <utility>

namespace core::rebase {

class RebasePlan {
public:
    static Result<RebasePlan> open(git::RepoSession& repo, git::RebaseInfo info, git_commit* root_commit);
    static RebasePlan open(git::RebaseInfo info, action::ActionsManager&& actions);

    struct MoveResult {
        bool changed                    = false;
        action::Action* last_unaffected = nullptr;
    };

    RebasePlan(RebasePlan&&) = default;

    RebasePlan(const RebasePlan&)            = delete;
    RebasePlan& operator=(const RebasePlan&) = delete;

    const action::Action* action_at(std::uint32_t row);

    template <action::action_type Act> action::Action& append(Act&& act) {
        m_revision += 1;
        return m_actions.append(std::forward(act));
    }

    MoveResult move_action(std::uint32_t from, std::uint32_t to);
    git::commit_t split(action::Action* action, git::commit_t&& prev, git::commit_t&& next);
    std::pair<git::commit_t, git::commit_t> merge_next(action::Action* act, git::commit_t&& commit);
    void set_type(action::Action* action, action::ActionType type);

    void clear();
    void clear_revisions();

    [[nodiscard]] git_commit* root_commit() const { return m_actions.get_root_commit(); }

    [[nodiscard]] std::size_t revision() const { return m_revision; }

    [[nodiscard]] auto begin() { return m_actions.begin(); }

    [[nodiscard]] auto end() { return m_actions.end(); }

    [[nodiscard]] auto begin() const { return m_actions.cbegin(); }

    [[nodiscard]] auto end() const { return m_actions.cend(); }

    [[nodiscard]] std::size_t size() const { return m_actions.size(); }

    [[nodiscard]] const git::RebaseInfo& info() const { return m_info; }

private:
    RebasePlan() = default;

    RebasePlan(git::RebaseInfo&& info, action::ActionsManager&& actions)
        : m_actions(std::move(actions))
        , m_info(std::move(info)) { }

    action::ActionsManager m_actions;
    git::RebaseInfo m_info;

    std::size_t m_revision = 0;
};

}
