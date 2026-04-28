#pragma once

#include "Action.h"
#include "ActionManager.h"
#include "core/conflict/ConflictManager.h"

namespace action {

/**
 * @brief Converts actions into a Git todo format and writes them to an output stream.
 */
class Converter {
public:
    Converter() = delete;

    /**
     * @brief Converts actions to a todo file format.
     *
     * @param output Output stream for the generated todo file.
     * @param manager Actions manager containing actions to convert.
     * @param conflict_manager Conflict manager used during conversion.
     * @param insert_break Whether to insert a break command.
     *
     * @return True if conversion succeeded, false otherwise.
     */
    static bool actions_to_todo(
        std::ostream& output,
        ActionsManager& manager,
        core::conflict::ConflictManager& conflict_manager,
        bool insert_break = false
    );
};

}
