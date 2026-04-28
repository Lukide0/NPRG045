#pragma once

#include "core/conflict/ConflictManager.h"
#include "core/git/types.h"

#include <optional>
#include <span>
#include <string>
#include <utility>

#include <git2/types.h>

namespace action {
class Action;

}

namespace core::conflict {

/**
 * @brief Represents the status of a conflict.
 */
enum class ConflictStatus {
    UNKNOWN,
    ERR,
    HAS_CONFLICT,
    NO_CONFLICT,
    RESOLVED_CONFLICT,
};

/**
 * @brief Converts ConflictStatus to a string.
 */
constexpr const char* conflict_to_string(ConflictStatus status) {
    switch (status) {
    case ConflictStatus::ERR:
        return "Error";
    case ConflictStatus::HAS_CONFLICT:
        return "Has conflict";
    case ConflictStatus::NO_CONFLICT:
        return "No conflict";
    case ConflictStatus::RESOLVED_CONFLICT:
        return "Resolved conflict";
    case ConflictStatus::UNKNOWN:
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
 * @brief Checks cherrypick result for a commit against its parent.
 *
 * @param commit Commit to check.
 * @param parent_commit Parent commit.
 *
 * @return Pair of conflict status and resulting index.
 */
std::pair<ConflictStatus, git::index_t> cherrypick_check(git_commit* commit, git_commit* parent_commit);

/**
 * @brief Checks cherrypick result for an action against its parent commit.
 *
 * @param act Action to check.
 * @param parent_commit Parent commit.
 *
 * @return Pair of conflict status and resulting index.
 */
std::pair<ConflictStatus, git::index_t> cherrypick_check(action::Action* act, git_commit* parent_commit);

/**
 * @brief Checks cherrypick result between two actions.
 *
 * @param act Current action.
 * @param parent_act Parent action.
 *
 * @return Pair of conflict status and resulting index.
 */
std::pair<ConflictStatus, git::index_t> cherrypick_check(action::Action* act, action::Action* parent_act);

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
 * @param conflict_action Conflict action.
 * @param repo Git repository.
 * @param files List of file OIDs.
 * @param callback Callback invoked per iteration.
 * @param payload User data passed to callback.
 *
 * @return True if iteration completed successfully.
 */
bool iterate_actions(
    action::Action& conflict_action,
    git_repository* repo,
    std::span<const git_oid> files,
    std::function<bool(bool, std::uint32_t, void*)> callback,
    void* payload
);

}
