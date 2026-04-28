#pragma once

#include "action/Action.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/types.h"
#include <filesystem>
#include <git2/types.h>
#include <string>
#include <utility>
#include <vector>

namespace core::state {

/**
 * @brief Serializable application state.
 */
struct SaveData {
    std::vector<std::pair<action::Action, std::string>> actions;
    std::vector<std::pair<conflict::ConflictEntry, std::string>> conflicts;
    std::vector<std::pair<conflict::ConflictTrees, core::git::tree_t>> conflict_trees;
    git::commit_t root;

    std::string head;
    std::string onto;
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
     * @param repo Repository path.
     * @param head Current HEAD reference.
     * @param onto Current ONTO reference.
     *
     * @return True if save succeeded.
     */
    static bool save(
        const std::filesystem::path& path,
        const std::filesystem::path& repo,
        const std::string& head,
        const std::string& onto
    );

    /**
     * @brief Loads application state from a file.
     *
     * @param path Input file path.
     * @param repo Loaded repository.
     *
     * @return Loaded state or std::nullopt on failure.
     */
    static std::optional<SaveData> load(const std::filesystem::path& path, git_repository** repo);
};

}
