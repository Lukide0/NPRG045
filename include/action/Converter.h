#pragma once

#include "Action.h"
#include "ActionManager.h"

class Converter {
public:
    Converter() = delete;

    static void actions_to_todo(std::ostream& output, const Action* actions, const ActionsManager& manager);
};
