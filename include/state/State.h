#pragma once

#include "core/action/Action.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/GitGraph.h"
#include "core/git/RepoSession.h"
#include "core/rebase/RebaseSession.h"

#include <filesystem>

#include <git2/types.h>

namespace state {

/**
 * @brief Serializable application state.
 */
struct SaveData {
    core::git::RepoSession repo     = core::git::RepoSession::invalid();
    core::git::GitGraph<void> graph = core::git::GitGraph<void>::empty();
    core::conflict::ConflictManager conflicts;
    core::git::RebaseInfo info;
    core::action::ActionsManager actions;
};

/**
 * @brief Handles persistence of application state.
 */
class State {
public:
    /**
     * @brief Saves application state to a file.
     *
     * @param path Output file path.
     * @param rebase The rebase session.
     *
     * @return True if save succeeded.
     */
    static bool save(const std::filesystem::path& path, const core::rebase::RebaseSession& rebase);

    /**
     * @brief Loads application state from a file.
     *
     * @param path Input file path.
     *
     * @return Loaded state
     */
    static core::Result<SaveData> load(const std::filesystem::path& path);
};

}
