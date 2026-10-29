#pragma once

#include "core/action/Action.h"
#include "core/action/ActionsManager.h"
#include "core/git/types.h"
#include <cstddef>
#include <cstdint>
#include <utility>

namespace core::rebase {

class RebaseStepView;

class RebasePlan {
public:
    struct MoveResult {
        bool changed                    = false;
        action::Action* last_unaffected = nullptr;
    };

    RebasePlan()                  = default;
    RebasePlan(const RebasePlan&) = delete;
    RebasePlan(RebasePlan&&)      = default;

    RebasePlan& operator=(const RebasePlan&) = delete;
    RebasePlan& operator=(RebasePlan&&)      = delete;

    const action::Action* action_at(std::uint32_t row);

    template <action::action_type Act> action::Action& append(Act&& act) {
        m_revision += 1;
        return m_actions.append(std::forward(act));
    }

    MoveResult move_action(std::uint32_t from, std::uint32_t to);
    git::commit_t split(action::Action* action, git::commit_t&& prev, git::commit_t&& next);
    std::pair<git::commit_t, git::commit_t> merge_next(action::Action* act, git::commit_t&& commit);

    void clear();
    void clear_revisions();

    [[nodiscard]] git_commit* root_commit() const { return m_actions.get_root_commit(); }

    [[nodiscard]] std::size_t revision() const { return m_revision; }

    [[nodiscard]] auto begin() const { return m_actions.begin(); }

    [[nodiscard]] auto end() const { return m_actions.end(); }

    [[nodiscard]] std::size_t size() const { return m_actions.size(); }

private:
    action::ActionsManager m_actions;

    std::size_t m_revision = 0;
};

}
