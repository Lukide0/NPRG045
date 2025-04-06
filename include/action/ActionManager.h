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

        auto to_it = m_action.begin();
        std::advance(to_it, to);

        if (from < to) {
            std::advance(to_it, 1);
        }

        m_action.splice(to_it, m_action, from_it);
    }

    void clear() {
        m_action.clear();
        m_msg.clear();
    }

    [[nodiscard]] const std::list<Action>& get_actions() const { return m_action; }

    std::list<Action>& get_actions() { return m_action; }

private:
    std::list<Action> m_action;
    std::vector<std::string> m_msg;
};

template <action_type Act> ActionsManager::ref_t ActionsManager::append(Act&& action) {
    return m_action.emplace_back(std::forward<Act>(action));
}
