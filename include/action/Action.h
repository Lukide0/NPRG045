#pragma once

#include "core/git/types.h"
#include "core/utils/optional_uint.h"
#include "core/utils/todo.h"
#include <git2/commit.h>
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
    Action(ActionType type, git_oid&& oid, git_repository* repo, optional_u31 msg_id = optional_u31::none())
        : m_oid(std::move(oid))
        , m_msg_id(msg_id)
        , m_type(type) {
        init_commit(repo);
    }

    Action(ActionType type, const git_oid& oid, git_repository* repo, optional_u31 msg_id = optional_u31::none())
        : m_oid(oid)
        , m_msg_id(msg_id)
        , m_type(type) {
        init_commit(repo);
    }

    Action* get_next() { return m_next; }

    [[nodiscard]] const Action* get_next() const { return m_next; }

    void set_next(Action* next) { m_next = next; }

    [[nodiscard]] const git_oid& get_oid() const { return m_oid; }

    [[nodiscard]] git_commit* get_commit() const { return m_commit.commit; }

    [[nodiscard]] optional_u31 get_msg_id() const { return m_msg_id; }

    [[nodiscard]] bool can_edit_msg() const {
        switch (m_type) {
        case ActionType::PICK:
        case ActionType::DROP:
        case ActionType::FIXUP:
        case ActionType::SQUASH:
            return false;
        case ActionType::REWORD:
        case ActionType::EDIT:
            return true;
        }

        return false;
    }

    [[nodiscard]] bool has_msg() const {
        switch (m_type) {
        case ActionType::PICK:
        case ActionType::DROP:
        case ActionType::FIXUP:
        case ActionType::SQUASH:
            return false;
        case ActionType::REWORD:
        case ActionType::EDIT:
            return true;
        }

        return false;
    }

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
    Action* m_next = nullptr;
    git_commit_t m_commit;
    git_oid m_oid;
    optional_u31 m_msg_id;
    ActionType m_type;

    void init_commit(git_repository* repo) { assert(git_commit_lookup(&m_commit.commit, repo, &m_oid) == 0); }
};
