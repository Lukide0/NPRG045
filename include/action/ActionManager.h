#pragma once

#include "Action.h"
#include <cstdint>
#include <git2/oid.h>
#include <iterator>
#include <list>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

template <typename T>
concept action_type = std::is_same_v<std::decay_t<T>, Action>;

class ActionsManager {

public:
    using ref_t  = Action&;
    using type_t = ActionType;

    ActionsManager()                      = default;
    ActionsManager(const ActionsManager&) = delete;
    ActionsManager(ActionsManager&&)      = delete;

    ActionsManager& operator=(ActionsManager&&)      = delete;
    ActionsManager& operator=(const ActionsManager&) = delete;

    template <action_type Act> ref_t append(Act&& action);

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

    void move(std::uint32_t from, std::uint32_t to) {
        auto from_it = m_action.begin();
        std::advance(from_it, from);

        Action* from_prev   = nullptr;
        Action* from_action = &*from_it;
        Action* from_next   = from_action->get_next();

        if (from_it != m_action.begin()) {
            from_prev = &*std::prev(from_it);
        }

        auto to_it = m_action.begin();
        std::advance(to_it, to);

        Action* to_prev   = nullptr;
        Action* to_action = &*to_it;
        Action* to_next   = to_it->get_next();

        if (to_it != m_action.begin()) {
            to_prev = &*std::prev(to_it);
        }

        if (from < to) {
            std::advance(to_it, 1);
        }

        m_action.splice(to_it, m_action, from_it);

        if (from_prev != nullptr) {
            from_prev->set_next(to_action);
        }

        if (to_prev != nullptr) {
            to_prev->set_next(from_action);
        }

        if (from + 1 == to) {
            to_action->set_next(from_action);
            from_action->set_next(to_next);
        } else if (to + 1 == from) {
            from_action->set_next(to_action);
            to_action->set_next(from_next);
        } else {
            to_action->set_next(from_next);
            from_action->set_next(to_next);
        }
    }

    void clear() {
        m_action.clear();
        m_msg.clear();
    }

    [[nodiscard]] const std::list<Action>& get_actions() const { return m_action; }

    std::list<Action>& get_actions() { return m_action; }

    static ActionsManager& get() {
        static ActionsManager manager;
        return manager;
    }

private:
    std::list<Action> m_action;
    std::vector<std::string> m_msg;
};

template <action_type Act> ActionsManager::ref_t ActionsManager::append(Act&& action) {
    auto& ref = m_action.emplace_back(std::forward<Act>(action));

    if (m_action.size() > 1) {
        auto prev = std::prev(m_action.end(), 2);
        prev->set_next(&ref);
    }

    return ref;
}
