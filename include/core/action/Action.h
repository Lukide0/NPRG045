#pragma once

#include "core/conflict/conflict.h"
#include "core/git/types.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <string>

#include <git2/commit.h>
#include <git2/oid.h>
#include <git2/types.h>
#include <utility>

namespace core::action {

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

constexpr bool is_valid_type(ActionType type) { return std::ranges::find(action_types, type) != action_types.end(); }

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

    std::unreachable();
}

/**
 * @brief Represents a single rebase action.
 */
class Action {
public:
    using ConflictStatus = conflict::ConflictState;

    /**
     * @brief Constructs an Action from an existing commit.
     *
     * @param type Type of the action.
     * @param commit Git commit object.
     * @param msg Optional message.
     */
    Action(ActionType type, git::commit_t&& commit, std::optional<std::string> msg = std::nullopt)
        : m_commit(std::move(commit))
        , m_edited_msg(std::move(msg))
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
     * @brief Gets commit OID.
     */
    [[nodiscard]] const git_oid& get_oid() const { return *git_commit_id(m_commit.get()); }

    /**
     * @brief Gets commit object.
     */
    [[nodiscard]] git_commit* get_commit() const { return m_commit.get(); }

    /**
     * @brief Gets optional message ID.
     */
    [[nodiscard]] auto get_msg() const { return m_edited_msg; }

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
            return m_edited_msg.has_value();
        }

        return false;
    }

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
     * @brief Sets message
     *
     * @param msg_id Message.
     */
    void set_msg(const std::string& msg) { m_edited_msg = msg; }

    void clear_msg() { m_edited_msg = std::nullopt; }

private:
    Action* m_next = nullptr;
    Action* m_prev = nullptr;
    git::commit_t m_commit;

    std::optional<std::string> m_edited_msg;
    ActionType m_type;

    void set_next(Action* next) { m_next = next; }

    void set_prev(Action* prev) { m_prev = prev; }

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

    friend ActionsManager;
};

}
