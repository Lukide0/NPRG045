#pragma once

#include "core/action/Action.h"
#include "core/action/ActionsManager.h"
#include "core/conflict/ConflictManager.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebaseSession.h"

#include <ostream>

namespace core::action {

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
     * @param session The rebase session.
     * @param result The rebase result.
     * @param insert_break Whether to insert a break command.
     *
     * @return True if conversion succeeded, false otherwise.
     */
    static bool actions_to_todo(
        std::ostream& output,
        core::rebase::RebaseSession& session,
        const core::rebase::RebaseAnalysis& result,
        bool insert_break = false
    );
};

}
