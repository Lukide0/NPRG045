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

enum class ConflictStatus {
    UNKNOWN,
    ERR,
    HAS_CONFLICT,
    NO_CONFLICT,
    RESOLVED_CONFLICT,
};

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

struct ResolutionResult {
    std::optional<std::string> err;
    git_oid id;
};

std::pair<ConflictStatus, git::index_t> cherrypick_check(git_commit* commit, git_commit* parent_commit);

std::pair<ConflictStatus, git::index_t> cherrypick_check(action::Action* act, git_commit* parent_commit);

std::pair<ConflictStatus, git::index_t> cherrypick_check(action::Action* act, action::Action* parent_act);

ResolutionResult add_resolved_files(
    git::index_t& index,
    git_repository* repo,
    std::span<const std::string> paths,
    std::span<const ConflictEntry> entries,
    ConflictManager& manager
);

bool iterate_actions(
    action::Action& conflict_action,
    git_repository* repo,
    std::span<const git_oid> files,
    std::function<bool(bool, std::uint32_t, void*)> callback,
    void* payload
);

}
