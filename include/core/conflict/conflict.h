#pragma once

#include "core/conflict/ConflictManager.h"
#include "core/git/types.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include <git2/oid.h>
#include <git2/types.h>

namespace core::rebase {

struct RebaseAnalysis;
class RebasePlan;

}

namespace core::conflict {

/**
 * @brief Represents the status of a conflict.
 */
enum class ConflictState {
    Unknown,
    Error,
    Conflict,
    Clean,
    Resolved,
};

/**
 * @brief Converts ConflictStatus to a string.
 */
constexpr const char* conflict_to_string(ConflictState status) {
    switch (status) {
    case ConflictState::Error:
        return "Error";
    case ConflictState::Conflict:
        return "Has conflict";
    case ConflictState::Clean:
        return "No conflict";
    case ConflictState::Resolved:
        return "Resolved conflict";
    case ConflictState::Unknown:
    default:
        return "Unknown";
    }
}

/**
 * @brief Result of a resolution operation.
 */
struct ResolutionResult {
    std::optional<std::string> err;
    git_oid id;
};

/**
 * @brief Cherry-picks the commit onto a tree in memory (nothing is written to the repository state).
 *
 * The ancestor is the commit's own parent, so a root commit uses an empty tree and a merge commit is an error.
 */
std::pair<ConflictState, git::index_t> pick_check(const git_commit* commit, const git_tree* onto_tree);

/**
 * @brief Applies resolved files to the index.
 *
 * @param index Git index.
 * @param repo Git repository.
 * @param paths File paths.
 * @param entries Conflict entries.
 * @param manager Conflict manager.
 *
 * @return Resolution result with optional error.
 */
ResolutionResult add_resolved_files(
    git::index_t& index,
    git_repository* repo,
    std::span<const std::string> paths,
    std::span<const ConflictEntry> entries,
    ConflictManager& manager
);

/**
 * @brief Iterates over conflict action and execute callback per item.
 *
 * @param repo Git repository.
 * @param plan Rebase plan.
 * @param result Rebase analysis with conflict.
 * @param callback Callback invoked per iteration.
 * @param payload User data passed to callback.
 *
 * @return True if iteration completed successfully.
 */
bool iterate_steps(
    git_repository* repo,
    const rebase::RebasePlan& plan,
    const rebase::RebaseAnalysis& result,
    std::function<bool(bool, std::uint32_t, void*)> callback,
    void* payload
);

}
