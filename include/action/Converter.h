#pragma once

#include "Action.h"
#include "ActionManager.h"
#include "core/conflict/ConflictManager.h"

namespace action {

class Converter {
public:
    Converter() = delete;

    static bool
    actions_to_todo(std::ostream& output, ActionsManager& manager, core::conflict::ConflictManager& conflict_manager);
};

}
