#pragma once

#include "core/conflict/conflict.h"
#include "core/git/types.h"
#include "core/utils/optional_uint.h"
#include "core/utils/todo.h"
#include <cassert>
#include <git2/commit.h>
#include <git2/oid.h>
#include <git2/types.h>
#include <utility>

namespace action {

class ActionsManager;

/**
 * @brief Types of rebase actions.
 */
enum class ActionType {
    PICK,
    DROP,
    SQUASH,
    FIXUP,
    REWORD,
    EDIT,
};

constexpr auto action_types = std::to_array<ActionType>({
    ActionType::PICK,
    ActionType::DROP,
    ActionType::SQUASH,
    ActionType::FIXUP,
    ActionType::REWORD,
    ActionType::EDIT,
});

/**
 * @brief Converts an ActionType to its string representation.
 */
static constexpr const char* type_to_str(ActionType type) {
    switch (type) {
    case ActionType::PICK:
        return "pick";
    case ActionType::DROP:
        return "drop";
    case ActionType::SQUASH:
        return "squash";
    case ActionType::FIXUP:
        return "fixup";
    case ActionType::REWORD:
        return "reword";
    case ActionType::EDIT:
        return "edit";
    }

    TODO("Unknown action type");
}

/**
 * @brief Represents a single rebase action.
 */
class Action {
public:
    using ConflictStatus = core::conflict::ConflictStatus;

    /**
     * @brief Constructs an Action from a Git commit OID.
     *
     * @param type Type of the action.
     * @param oid Commit object ID.
     * @param repo Git repository.
     * @param msg_id Optional message ID.
     */
    Action(ActionType type, const git_oid& oid, git_repository* repo, optional_u31 msg_id = optional_u31::none())
        : m_msg_id(msg_id)
        , m_type(type) {
        init_commit(repo, oid);
    }

    /**
     * @brief Constructs an Action from an existing commit.
     *
     * @param type Type of the action.
     * @param commit Git commit object.
     * @param msg_id Optional message ID.
     */
    Action(ActionType type, core::git::commit_t&& commit, optional_u31 msg_id = optional_u31::none())
        : m_commit(std::move(commit))
        , m_msg_id(msg_id)
        , m_type(type) { }

    /**
     * @brief Gets next action.
     */
    Action* get_next() { return m_next; }

    [[nodiscard]] const Action* get_next() const { return m_next; }

    /**
     * @brief Gets previous action.
     */
    Action* get_prev() { return m_prev; }

    [[nodiscard]] const Action* get_prev() const { return m_prev; }

    /**
     * @brief Sets next action connection.
     *
     * @param next Next action.
     */
    void set_next_connection(Action* next) {
        m_next = next;
        if (m_next != nullptr) {
            m_next->set_prev(this);
        }
    }

    /**
     * @brief Sets previous action connection.
     *
     * @param prev Previous action.
     */
    void set_prev_connection(Action* prev) {
        m_prev = prev;
        if (m_prev != nullptr) {
            m_prev->set_next(this);
        }
    }

    /**
     * @brief Gets commit OID.
     */
    [[nodiscard]] const git_oid& get_oid() const { return *git_commit_id(m_commit.get()); }

    /**
     * @brief Gets commit object.
     */
    [[nodiscard]] git_commit* get_commit() { return m_commit.get(); }

    [[nodiscard]] const git_commit* get_commit() const { return m_commit.get(); }

    /**
     * @brief Gets optional message ID.
     */
    [[nodiscard]] optional_u31 get_msg_id() const { return m_msg_id; }

    /**
     * @brief Checks if message can be edited.
     */
    [[nodiscard]] bool can_edit_msg() const {
        switch (m_type) {
        case ActionType::PICK:
        case ActionType::DROP:
        case ActionType::FIXUP:
        case ActionType::SQUASH:
        case ActionType::EDIT:
            return false;
        case ActionType::REWORD:
            return true;
        }

        return false;
    }

    /**
     * @brief Checks if action has a message.
     */
    [[nodiscard]] bool has_msg() const {
        switch (m_type) {
        case ActionType::PICK:
        case ActionType::DROP:
        case ActionType::FIXUP:
        case ActionType::SQUASH:
        case ActionType::EDIT:
            return false;
        case ActionType::REWORD:
            return true;
        }

        return false;
    }

    /**
     * @brief Clears tree data and resets conflict status.
     */
    void clear_tree() {
        m_tree.destroy();
        m_tree_conflict = ConflictStatus::UNKNOWN;
    }

    /**
     * @brief Sets tree and its conflict status.
     *
     * @param tree Git tree object.
     * @param status Conflict status.
     */
    void set_tree(core::git::tree_t&& tree, ConflictStatus status) {
        m_tree          = std::move(tree);
        m_tree_conflict = status;
    }

    /**
     * @brief Gets tree object.
     */
    git_tree* get_tree() { return m_tree; }

    [[nodiscard]] const git_tree* get_tree() const { return m_tree.get(); }

    /**
     * @brief Sets tree conflict status.
     *
     * @param status Conflict status.
     */
    void set_tree_status(ConflictStatus status) { m_tree_conflict = status; }

    /**
     * @brief Gets tree conflict status.
     */
    [[nodiscard]] ConflictStatus get_tree_status() const { return m_tree_conflict; }

    /**
     * @brief Gets action type.
     */
    [[nodiscard]] ActionType get_type() const { return m_type; }

    /**
     * @brief Sets action type.
     *
     * @param type New action type.
     */
    void set_type(ActionType type) { m_type = type; }

    /**
     * @brief Sets message ID.
     *
     * @param msg_id Message ID.
     */
    void set_msg_id(optional_u31 msg_id) { m_msg_id = msg_id; }

private:
    Action* m_next = nullptr;
    Action* m_prev = nullptr;
    core::git::commit_t m_commit;

    core::git::tree_t m_tree;
    ConflictStatus m_tree_conflict = ConflictStatus::UNKNOWN;

    optional_u31 m_msg_id;
    ActionType m_type;

    void set_next(Action* next) { m_next = next; }

    void set_prev(Action* prev) { m_prev = prev; }

    void init_commit(git_repository* repo, const git_oid& oid) {
        const bool status = git_commit_lookup(&m_commit, repo, &oid) == 0;
        assert(status && m_commit != nullptr);
    }

    friend ActionsManager;
};

}
