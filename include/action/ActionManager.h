#pragma once

#include "Action.h"
#include "core/git/types.h"
#include "core/utils/unexpected.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <git2/oid.h>
#include <git2/types.h>

namespace action {

/**
 * @brief Concept that restricts types to Action.
 */
template <typename T>
concept action_type = std::is_same_v<std::decay_t<T>, Action>;

/**
 * @brief Iterator over Action linked list.
 */
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

/**
 * @brief Manages a linked list of Actions and associated commit messages.
 */
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

    /**
     * @brief Appends a new action to the list.
     *
     * @tparam Act Action type.
     * @param action Action to append.
     *
     * @return Reference to created action.
     */
    template <action_type Act> Action& append(Act&& action);

    /**
     * @brief Gets message by index.
     *
     * @param index Message index.
     */
    [[nodiscard]] const std::string& get_msg(std::uint32_t index) const { return m_msg[index]; }

    std::string& get_msg(std::uint32_t index) { return m_msg[index]; }

    /**
     * @brief Adds a copy of a message.
     *
     * @param msg Message text.
     *
     * @return Index of inserted message.
     */
    std::uint32_t add_msg(const std::string& msg);

    std::uint32_t add_msg(std::string&& msg);

    /**
     * @brief Splits an action into two actions.
     */
    core::git::commit_t split(Action* act, core::git::commit_t&& prev, core::git::commit_t&& next);

    /**
     * @brief Merges an action with its next action.
     */
    std::pair<core::git::commit_t, core::git::commit_t> merge_next(Action* act, core::git::commit_t&& commit);

    /**
     * @brief Moves an action within the list.
     *
     * @param from Source index.
     * @param to Destination index.
     *
     * @return Neighboring action after move.
     */
    Action* move(std::uint32_t from, std::uint32_t to);

    /**
     * @brief Clears all actions and messages.
     */
    void clear();

    /**
     * @brief Gets head action.
     */
    [[nodiscard]] const Action* get_actions() const { return m_head; }

    Action* get_actions() { return m_head; }

    iterator_t begin() { return { m_head }; }

    iterator_t end() { return { nullptr }; }

    [[nodiscard]] const_iterator_t begin() const { return { m_head }; }

    [[nodiscard]] const_iterator_t end() const { return { nullptr }; }

    [[nodiscard]] const_iterator_t cbegin() const { return { m_head }; }

    [[nodiscard]] const_iterator_t cend() const { return { nullptr }; }

    /**
     * @brief Gets index of iterator.
     *
     * @param iter Iterator.
     *
     * @return Index in list.
     */
    [[nodiscard]] std::size_t get_index(const_iterator_t iter) const;

    /**
     * @brief Gets root commit.
     */
    git_commit* get_root_commit() { return m_root_commit; }

    /**
     * @brief Gets first action.
     */
    Action* get_first_action() { return m_head; }

    /**
     * @brief Sets root commit.
     *
     * @param commit Root commit.
     */
    void set_root_commit(git_commit* commit) { m_root_commit = commit; }

    /**
     * @brief Swaps commits of an action.
     *
     * @param act Target action.
     * @param commit Commit to swap.
     */
    static void swap_commits(Action* act, core::git::commit_t& commit) { std::swap(act->m_commit, commit); }

    /**
     * @brief Gets global manager instance.
     */
    static ActionsManager& get() {
        static ActionsManager manager;
        return manager;
    }

    /**
     * @brief Gets parent commit of an action.
     */
    static git_commit* get_parent_commit(Action* act);

    /**
     * @brief Gets nearest picked parent action.
     *
     * @return If the picked parent does not exist then the nullptr is returned.
     */
    static Action* get_picked_parent(Action* act);

    /**
     * @brief Gets action by index.
     */
    Action* get_action(std::uint32_t index);

    /**
     * @brief Gets index of action.
     */
    std::uint32_t get_action_index(Action* find);

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
