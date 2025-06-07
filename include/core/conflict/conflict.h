#pragma once

#include "core/conflict/conflict_iterator.h"
#include "core/git/types.h"
#include <git2/types.h>
#include <utility>

namespace core::conflict {

enum class ConflictStatus {
    ERR,
    HAS_CONFLICT,
    NO_CONFLICT,
};

std::pair<ConflictStatus, git::index_t> cherrypick_check(git_commit* commit, git_commit* parent_commit);

}
