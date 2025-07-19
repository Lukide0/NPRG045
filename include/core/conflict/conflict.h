#pragma once

#include "action/ActionManager.h"
#include "App.h"
#include "core/git/types.h"
#include "core/state/Command.h"

#include <span>
#include <string>
#include <utility>

#include <git2/types.h>

namespace core::conflict {

enum class ConflictStatus {
    ERR,
    HAS_CONFLICT,
    NO_CONFLICT,
};

struct ResolutionResult {
    std::optional<std::string> err;
    git_oid id;
};

std::pair<ConflictStatus, git::index_t> cherrypick_check(git_commit* commit, git_commit* parent_commit);

ResolutionResult add_resolved_files(
    git::index_t& index,
    git_repository* repo,
    std::span<const std::string> paths,
    std::span<const ConflictEntry> entries,
    ConflictManager& manager
);

}
