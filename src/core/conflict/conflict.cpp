#include "core/conflict/conflict.h"
#include "core/conflict/conflict_iterator.h"
#include "core/git/types.h"
#include <git2/blob.h>
#include <git2/cherrypick.h>
#include <git2/commit.h>
#include <git2/index.h>
#include <git2/repository.h>
#include <git2/types.h>
#include <utility>

namespace core::conflict {

std::pair<ConflictStatus, git::index_t> cherrypick_check(git_commit* commit, git_commit* parent_commit) {

    git::index_t index;

    git_repository* repo = git_commit_owner(commit);

    if (git_cherrypick_commit(&index, repo, commit, parent_commit, 0, nullptr) != 0) {
        return std::make_pair(ConflictStatus::ERR, std::move(index));
    }

    if (git_index_has_conflicts(index) != 0) {
        return std::make_pair(ConflictStatus::HAS_CONFLICT, std::move(index));
    } else {
        return std::make_pair(ConflictStatus::NO_CONFLICT, std::move(index));
    }
}

std::pair<bool, git_oid>
add_resolved_files(git::index_t& index, git_repository* repo, const std::vector<std::string>& paths) {
    git_oid oid;

    bool ok = true;
    for (auto&& path : paths) {
        ok = git_index_add_bypath(index.get(), path.c_str()) == 0;
        break;
    }

    if (ok) {
        ok = git_index_write_tree_to(&oid, index.get(), repo) == 0;
    }

    return std::make_pair(ok, oid);
}

}
