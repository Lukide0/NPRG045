#include "action/ActionManager.h"

#include "action/Action.h"
#include "core/git/types.h"
#include "core/utils/unexpected.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <git2/oid.h>
#include <git2/types.h>

namespace action {

std::uint32_t ActionsManager::add_msg(const std::string& msg) {
    std::uint32_t index = m_msg.size();
    m_msg.emplace_back(msg);
    return index;
}

std::uint32_t ActionsManager::add_msg(std::string&& msg) {
    std::uint32_t index = m_msg.size();
    m_msg.emplace_back(std::move(msg));
    return index;
}

core::git::commit_t ActionsManager::split(Action* act, core::git::commit_t&& prev, core::git::commit_t&& next) {

    core::git::commit_t commit = std::move(act->m_commit);

    act->m_commit = std::move(prev);

    auto* tmp = new Action(act->get_type(), std::move(next));

    auto* next_act = act->get_next();

    tmp->set_next_connection(next_act);
    tmp->set_prev_connection(act);

    return commit;
}

std::pair<core::git::commit_t, core::git::commit_t>
ActionsManager::merge_next(Action* act, core::git::commit_t&& commit) {
    assert(act->get_next() != nullptr);

    auto* next = act->get_next();

    auto commits
        = std::make_pair<core::git::commit_t, core::git::commit_t>(std::move(act->m_commit), std::move(next->m_commit));

    act->m_commit = std::move(commit);

    act->set_next_connection(next->get_next());

    delete next;

    return commits;
}

Action* ActionsManager::move(std::uint32_t from, std::uint32_t to) {
    if (from == to) {
        return m_tail;
    }

    auto* from_ptr = get_action(from);
    auto* to_ptr   = get_action(to);

    auto* from_prev = from_ptr->get_prev();
    auto* from_next = from_ptr->get_next();

    auto* to_prev = to_ptr->get_prev();
    auto* to_next = to_ptr->get_next();

    if (from + 1 == to) {
        // to <-> from
        from_ptr->set_prev_connection(to_ptr);

        // from <-> next
        from_ptr->set_next_connection(to_next);

        // prev <-> to
        to_ptr->set_prev_connection(from_prev);

        if (m_tail == to_ptr) {
            m_tail = from_ptr;
        }

        if (m_head == from_ptr) {
            m_head = to_ptr;
        }

        return from_prev;

    } else if (from == to + 1) {
        // prev <-> from
        from_ptr->set_prev_connection(to_prev);

        // from <-> to
        from_ptr->set_next_connection(to_ptr);

        // to <-> next
        to_ptr->set_next_connection(from_next);

        if (m_tail == from_ptr) {
            m_tail = to_ptr;
        }

        if (m_head == to_ptr) {
            m_head = from_ptr;
        }

        return to_prev;

    } else if (to < from) {
        // from <-> to
        from_ptr->set_next_connection(to_ptr);

        // prev <-> from
        from_ptr->set_prev_connection(to_prev);

        from_prev->set_next_connection(from_next);

        if (m_tail == from_ptr) {
            m_tail = from_prev;
        }

        if (m_head == to_ptr) {
            m_head = from_ptr;
        }

        return to_prev;

    } else if (to > from) {
        // to <-> from
        from_ptr->set_prev_connection(to_ptr);

        // from <-> next
        from_ptr->set_next_connection(to_next);

        from_next->set_prev_connection(from_prev);

        if (m_tail == to_ptr) {
            m_tail = from_ptr;
        }

        if (m_head == from_ptr) {
            m_head = from_next;
        }

        return from_prev;
    }

    // unreachable
    UNEXPECTED();
}

void ActionsManager::clear() {
    for (auto* ptr = m_head; ptr != nullptr;) {
        auto* p = ptr;
        ptr     = ptr->get_next();

        delete p;
    }

    m_msg.clear();
    m_head = nullptr;
    m_tail = nullptr;
}

[[nodiscard]] std::size_t ActionsManager::get_index(const_iterator_t iter) const {
    std::size_t i = 0;

    for (auto it = cbegin(); it != iter; ++it) {
        i += 1;
    }

    return i;
}

git_commit* ActionsManager::get_parent_commit(Action* act) {
    if (act == nullptr) {
        return nullptr;
    }

    auto* parent = act->get_prev();
    if (parent == nullptr) {
        return get().get_root_commit();
    } else {
        return parent->get_commit();
    }
}

Action* ActionsManager::get_picked_parent(Action* act) {
    if (act == nullptr) {
        return nullptr;
    }

    auto* parent = act->get_prev();
    while (parent != nullptr) {
        switch (parent->get_type()) {
        case ActionType::DROP:
            break;
        case ActionType::SQUASH:
        case ActionType::FIXUP:
        case ActionType::PICK:
        case ActionType::REWORD:
        case ActionType::EDIT:
            return parent;
        }

        parent = parent->get_prev();
    }

    return nullptr;
}

Action* ActionsManager::get_action(std::uint32_t index) {
    auto* act = m_head;

    for (std::uint32_t i = 0; i < index && act != nullptr; ++i) {
        act = act->get_next();
    }

    return act;
}

std::uint32_t ActionsManager::get_action_index(Action* find) {
    auto* act           = m_head;
    std::uint32_t index = 0;

    for (; act != find && act != nullptr; ++index) {
        act = act->get_next();
    }

    return index;
}
}
