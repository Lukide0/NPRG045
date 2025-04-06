#pragma once

#include "core/utils/optional_uint.h"
#include "core/utils/todo.h"
#include <git2/oid.h>
#include <utility>

enum class ActionType {
    PICK,
    DROP,
    SQUASH,
    FIXUP,
    REWORD,
    EDIT,
};

class Action {
public:
    Action(ActionType type, git_oid&& oid, optional_u31 msg_id = optional_u31::none())
        : m_oid(std::move(oid))
        , m_msg_id(msg_id)
        , m_type(type) { }

    Action(ActionType type, const git_oid& oid, optional_u31 msg_id = optional_u31::none())
        : m_oid(oid)
        , m_msg_id(msg_id)
        , m_type(type) { }

    [[nodiscard]] const git_oid& get_oid() const { return m_oid; }

    [[nodiscard]] optional_u31 get_msg_id() const { return m_msg_id; }

    [[nodiscard]] ActionType get_type() const { return m_type; }

    void set_type(ActionType type) { m_type = type; }

    void set_msg_id(optional_u31 msg_id) { m_msg_id = msg_id; }

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

private:
    git_oid m_oid;
    optional_u31 m_msg_id;
    ActionType m_type;
};
