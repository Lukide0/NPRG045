#pragma once

#include "Action.h"
#include "ActionManager.h"
#include <concepts>
#include <iterator>
#include <list>

class Converter {
public:
    using action_iter_t = std::list<Action>::const_iterator;
    Converter()         = delete;

    static void actions_to_todo(std::ostream& output, const std::list<Action>& actions, const ActionsManager& manager) {
        actions_to_todo(output, actions.begin(), actions.end(), manager);
    }

    static void
    actions_to_todo(std::ostream& output, action_iter_t begin, action_iter_t end, const ActionsManager& manager);
};
