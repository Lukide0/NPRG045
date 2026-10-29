#include "core/rebase/RebasePlan.h"
#include "core/action/Action.h"
#include "core/git/types.h"
#include <cstdint>
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

void RebasePlan::clear() {
    m_actions.clear();
    m_revision += 1;
}

void RebasePlan::clear_revisions() { m_revision = 0; }
}
