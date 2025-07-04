#pragma once

#include "Action.h"
#include "core/git/types.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <git2/oid.h>
#include <git2/types.h>

namespace action {

template <typename T>
concept action_type = std::is_same_v<std::decay_t<T>, Action>;

template <bool Contant> class ActionIterator {
private:
    using value_t = std::conditional_t<Contant, const Action*, Action*>;
    using ref_t   = std::conditional_t<Contant, const Action&, Action&>;

public:
    ActionIterator(value_t action)
        : m_action(action) { }

    ActionIterator& operator++() {
        m_action = m_action->get_next();
        return *this;
    }

    ActionIterator& operator--() {
        m_action = m_action->get_prev();
        return *this;
    }

    value_t operator->() const { return m_action; }

    ref_t operator*() const { return *m_action; }

    bool operator==(const ActionIterator& other) const { return m_action == other.m_action; }

    ActionIterator& operator=(const ActionIterator& other) = default;

private:
    value_t m_action;
};

class ActionsManager {
public:
    using type_t           = ActionType;
    using iterator_t       = ActionIterator<false>;
    using const_iterator_t = ActionIterator<true>;

    ActionsManager()                      = default;
    ActionsManager(const ActionsManager&) = delete;
    ActionsManager(ActionsManager&&)      = delete;

    ~ActionsManager() { clear(); }

    ActionsManager& operator=(ActionsManager&&)      = delete;
    ActionsManager& operator=(const ActionsManager&) = delete;

    template <action_type Act> Action& append(Act&& action);

    [[nodiscard]] const std::string& get_msg(std::uint32_t index) const { return m_msg[index]; }

    std::string& get_msg(std::uint32_t index) { return m_msg[index]; }

    std::uint32_t add_msg(const std::string& msg) {
        std::uint32_t index = m_msg.size();
        m_msg.emplace_back(msg);
        return index;
    }

    std::uint32_t add_msg(std::string&& msg) {
        std::uint32_t index = m_msg.size();
        m_msg.emplace_back(std::move(msg));
        return index;
    }

    // TODO: Fix messages
    core::git::commit_t split(Action* act, core::git::commit_t&& prev, core::git::commit_t&& next) {

        core::git::commit_t commit = std::move(act->m_commit);

        act->m_commit = std::move(prev);

        auto* tmp = new Action(act->get_type(), std::move(next));

        auto* next_act = act->get_next();

        tmp->set_next_connection(next_act);
        tmp->set_prev_connection(act);

        return commit;
    }

    // TODO: Fix messages
    std::pair<core::git::commit_t, core::git::commit_t> merge_next(Action* act, core::git::commit_t&& commit) {
        assert(act->get_next() != nullptr);

        auto* next = act->get_next();

        auto commits = std::make_pair<core::git::commit_t, core::git::commit_t>(
            std::move(act->m_commit), std::move(next->m_commit)
        );

        act->m_commit = std::move(commit);

        act->set_next_connection(next->get_next());

        delete next;

        return commits;
    }

    void move(std::uint32_t from, std::uint32_t to) {
        if (from == to) {
            return;
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
        }
    }

    void clear() {
        for (auto* ptr = m_head; ptr != nullptr;) {
            auto* p = ptr;
            ptr     = ptr->get_next();

            delete p;
        }

        m_msg.clear();
        m_head = nullptr;
        m_tail = nullptr;
    }

    [[nodiscard]] const Action* get_actions() const { return m_head; }

    Action* get_actions() { return m_head; }

    iterator_t begin() { return { m_head }; }

    iterator_t end() { return { nullptr }; }

    [[nodiscard]] const_iterator_t begin() const { return { m_head }; }

    [[nodiscard]] const_iterator_t end() const { return { nullptr }; }

    [[nodiscard]] const_iterator_t cbegin() const { return { m_head }; }

    [[nodiscard]] const_iterator_t cend() const { return { nullptr }; }

    [[nodiscard]] std::size_t get_index(const_iterator_t iter) const {
        std::size_t i = 0;

        for (auto it = cbegin(); it != iter; ++it) {
            i += 1;
        }

        return i;
    }

    git_commit* get_root_commit() { return m_root_commit; }

    void set_root_commit(git_commit* commit) { m_root_commit = commit; }

    static void swap_commits(Action* act, core::git::commit_t& commit) { std::swap(act->m_commit, commit); }

    static ActionsManager& get() {
        static ActionsManager manager;
        return manager;
    }

    static git_commit* get_parent_commit(Action* act) {
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

    static git_commit* get_picked_parent_commit(Action* act) {
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
                return parent->get_commit();
            }

            parent = parent->get_prev();
        }

        return get().get_root_commit();
    }

    Action* get_action(std::uint32_t index) {
        auto* act = m_head;

        for (std::uint32_t i = 0; i < index && act != nullptr; ++i) {
            act = act->get_next();
        }

        return act;
    }

    std::uint32_t get_action_index(Action* find) {
        auto* act           = m_head;
        std::uint32_t index = 0;

        for (; act != find && act != nullptr; ++index) {
            act = act->get_next();
        }

        return index;
    }

private:
    Action* m_head = nullptr;
    Action* m_tail = nullptr;

    std::vector<std::string> m_msg;

    git_commit* m_root_commit = nullptr;
};

template <action_type Act> Action& ActionsManager::append(Act&& action) {
    auto* ptr = new Action(std::forward<Act>(action));

    // tail <-> ptr
    ptr->set_prev_connection(m_tail);

    if (m_head == nullptr) {
        m_head = ptr;
    }
    m_tail = ptr;

    return *ptr;
}

}
