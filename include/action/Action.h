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
    using ConflictStatus = core::conflict::ConflictStatus;

    Action(ActionType type, const git_oid& oid, git_repository* repo, optional_u31 msg_id = optional_u31::none())
        : m_msg_id(msg_id)
        , m_type(type) {
        init_commit(repo, oid);
    }

    Action(ActionType type, core::git::commit_t&& commit, optional_u31 msg_id = optional_u31::none())
        : m_commit(std::move(commit))
        , m_msg_id(msg_id)
        , m_type(type) { }

    Action* get_next() { return m_next; }

    [[nodiscard]] const Action* get_next() const { return m_next; }

    Action* get_prev() { return m_prev; }

    [[nodiscard]] const Action* get_prev() const { return m_prev; }

    void set_next_connection(Action* next) {
        m_next = next;
        if (m_next != nullptr) {
            m_next->set_prev(this);
        }
    }

    void set_prev_connection(Action* prev) {
        m_prev = prev;
        if (m_prev != nullptr) {
            m_prev->set_next(this);
        }
    }

    [[nodiscard]] const git_oid& get_oid() const { return *git_commit_id(m_commit.get()); }

    [[nodiscard]] git_commit* get_commit() { return m_commit.get(); }

    [[nodiscard]] const git_commit* get_commit() const { return m_commit.get(); }

    [[nodiscard]] optional_u31 get_msg_id() const { return m_msg_id; }

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

    void clear_tree() {
        m_tree.destroy();
        m_tree_conflict = ConflictStatus::UNKNOWN;
    }

    void set_tree(core::git::tree_t&& tree, ConflictStatus status) {
        m_tree          = std::move(tree);
        m_tree_conflict = status;
    }

    git_tree* get_tree() { return m_tree; }

    void set_tree_status(ConflictStatus status) { m_tree_conflict = status; }

    [[nodiscard]] ConflictStatus get_tree_status() const { return m_tree_conflict; }

    [[nodiscard]] const git_tree* get_tree() const { return m_tree.get(); }

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
