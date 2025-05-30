#include "action/Converter.h"
#include "action/Action.h"
#include "action/ActionManager.h"
#include "core/utils/todo.h"

#include <git2/commit.h>
#include <git2/oid.h>

#include <array>
#include <cassert>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>

namespace action {

class ActionInfo {
private:
    std::array<char, 256> m_buff;
    const char* m_oid;
    const char* m_msg;

public:
    ActionInfo(const Action& act) {
        m_oid = git_oid_tostr(m_buff.data(), m_buff.size(), &act.get_oid());
        m_msg = git_commit_summary(const_cast<git_commit*>(act.get_commit()));
    }

    [[nodiscard]] const char* msg() const { return m_msg; }

    [[nodiscard]] const char* oid() const { return m_oid; }

    [[nodiscard]] bool has_error() const { return m_msg == nullptr || m_oid == nullptr; }
};

void pick_to_todo(std::ostream& output, const Action& act, const ActionsManager& /*unused*/) {
    auto info = ActionInfo(act);

    assert(!info.has_error());

    output << "pick " << info.oid() << ' ' << info.msg() << '\n';

    if (const Action* next = act.get_next()) {
        switch (next->get_type()) {
        case ActionType::SQUASH:
            TODO("Report to the user that the squash commit must have 'reword' parent");
            break;
        case ActionType::PICK:
        case ActionType::DROP:
        case ActionType::FIXUP:
        case ActionType::REWORD:
        case ActionType::EDIT:
            break;
        }
    }
}

void drop_to_todo(std::ostream& output, const Action& act, const ActionsManager& /*unused*/) {
    auto info = ActionInfo(act);

    assert(!info.has_error());

    output << "drop " << info.oid() << ' ' << info.msg() << '\n';
}

void edit_to_todo(std::ostream& output, const Action& act, const ActionsManager& /*unused*/) {
    auto info = ActionInfo(act);

    assert(!info.has_error());

    output << "drop " << info.oid() << ' ' << info.msg() << '\n';
}

void reword_to_todo(std::ostream& output, const Action& act, const ActionsManager& manager) {
    auto info = ActionInfo(act);

    assert(!info.has_error());

    // 1. pick the commit
    output << "pick " << info.oid() << ' ' << info.msg() << '\n';

    // The commit message is not changed
    if (!act.has_msg()) {
        return;
    }

    auto msg_id = act.get_msg_id();
    assert(msg_id.is_value());

    const std::string& msg = manager.get_msg(msg_id.value());

    std::istringstream text(msg);

    std::string line;

    if (!std::getline(text, line)) {
        TODO("Report to the user that the message is empty");
    }

    // 2. reword commit
    output << "exec " << "git commit --amend --only" << " -m " << std::quoted(line);

    while (std::getline(text, line)) {
        output << " -m " << std::quoted(line);
    }

    output << '\n';
}

void squash_to_todo(std::ostream& output, const Action& act, const ActionsManager& /*unused*/) {
    auto info = ActionInfo(act);

    assert(!info.has_error());

    // NOTE: The
    output << "fixup " << info.oid() << ' ' << info.msg() << '\n';
}

void fixup_to_todo(std::ostream& output, const Action& act, const ActionsManager& /*unused*/) {
    auto info = ActionInfo(act);

    assert(!info.has_error());

    output << "fixup " << info.oid() << ' ' << info.msg() << '\n';
}

void Converter::actions_to_todo(std::ostream& output, const Action* actions, const ActionsManager& manager) {
    for (const auto* ptr = actions; ptr != nullptr; ptr = ptr->get_next()) {
        const Action& act = *ptr;

        switch (act.get_type()) {
        case ActionType::PICK:
            pick_to_todo(output, act, manager);
            break;

        case ActionType::DROP:
            drop_to_todo(output, act, manager);
            break;

        case ActionType::SQUASH:
            squash_to_todo(output, act, manager);
            break;

        case ActionType::FIXUP:
            fixup_to_todo(output, act, manager);
            break;

        case ActionType::REWORD:
            reword_to_todo(output, act, manager);
            break;

        case ActionType::EDIT:
            edit_to_todo(output, act, manager);
            break;
        }
    }
}

}
