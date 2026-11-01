#include "core/conflict/conflict.h"

#include "core/action/Action.h"
#include "core/action/ActionsManager.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/error.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebasePlan.h"
#include "core/rebase/RebaseSession.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <git2/errors.h>
#include <git2/sys/errors.h>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <git2/blob.h>
#include <git2/cherrypick.h>
#include <git2/commit.h>
#include <git2/diff.h>
#include <git2/index.h>
#include <git2/merge.h>
#include <git2/oid.h>
#include <git2/repository.h>
#include <git2/status.h>
#include <git2/strarray.h>
#include <git2/types.h>

namespace core::conflict {

using Result = std::pair<ConflictState, git::index_t>;

Result drop_check(git_tree* onto_tree) {
    git::index_t index;

    if (git_index_new(&index) != 0 || git_index_read_tree(index, onto_tree) != 0) {
        return { ConflictState::Error, std::move(index) };
    }

    return { ConflictState::Clean, std::move(index) };
}

Result pick_check(const git_commit* commit, const git_tree* onto_tree) {
    git::index_t index;
    git::tree_t act_tree;
    git::tree_t ancestor_tree;

    if (git_commit_tree(&act_tree, commit) != 0) {
        return { ConflictState::Error, std::move(index) };
    }

    switch (git_commit_parentcount(commit)) {
    case 0:
        break;
    case 1: {
        git::commit_t ancestor_commit;
        if (git_commit_parent(&ancestor_commit, commit, 0) != 0
            || git_commit_tree(&ancestor_tree, ancestor_commit) != 0) {
            return { ConflictState::Error, std::move(index) };
        }
        break;
    }
    // merge commit
    default:
        // set custom error
        git_error_set(GIT_ERROR_INVALID, "Merge commit are not supported");
        return { ConflictState::Error, std::move(index) };
    }

    if (git_merge_trees(&index, git_commit_owner(commit), ancestor_tree, onto_tree, act_tree, nullptr) != 0) {
        return { ConflictState::Error, std::move(index) };
    }

    ConflictState status = git_index_has_conflicts(index) != 0 ? ConflictState::Conflict : ConflictState::Clean;

    return { status, std::move(index) };
}

Result action_check(action::Action* act, git_tree* onto_tree) {
    switch (act->get_type()) {
    case action::ActionType::PICK:
    case action::ActionType::SQUASH:
    case action::ActionType::FIXUP:
    case action::ActionType::REWORD:
    case action::ActionType::EDIT:
        return pick_check(act->get_commit(), onto_tree);
    case action::ActionType::DROP:
        break;
    }

    return drop_check(onto_tree);
}

ResolutionResult add_resolved_files(
    git::index_t& index,
    git_repository* repo,
    std::span<const std::string> paths,
    std::span<const ConflictEntry> entries,
    ConflictManager& manager
) {
    assert(paths.size() == entries.size());

    ResolutionResult res;

    git::status_list_t list;

    git_status_options opts = GIT_STATUS_OPTIONS_INIT;
    opts.show               = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX
        | GIT_STATUS_OPT_SORT_CASE_SENSITIVELY;

    // shortcut
    git_strarray& ps = opts.pathspec;

    git::str_array str_storage(paths);
    str_storage.fill(ps);

    if (git_status_list_new(&list, repo, &opts) != 0) {
        res.err = git::get_last_error();
        return res;
    }

    std::size_t n = git_status_list_entrycount(list.get());
    for (std::size_t i = 0; i < n; ++i) {
        const git_status_entry* entry = git_status_byindex(list, i);
        const auto status             = entry->status;

        constexpr auto status_mask
            = GIT_STATUS_INDEX_NEW | GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_INDEX_DELETED | GIT_STATUS_INDEX_RENAMED;

        if ((status & status_mask) == 0) {
            res.err = "No changes have been staged for resolution";
            return res;
        }
    }

    if (res.err.has_value()) {
        return res;
    }

    // record resolutions
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        const auto& path  = paths[i];

        const git_index_entry* index_entry = git_index_get_bypath(index, path.c_str(), GIT_INDEX_STAGE_NORMAL);
        // not found in index
        if (index_entry == nullptr) {
            // deleted
            manager.add_resolution(entry, "");
            continue;
        }

        std::string id = git_oid_tostr_s(&index_entry->id);
        manager.add_resolution(entry, id);
    }

    if (git_index_write_tree_to(&res.id, index.get(), repo) != 0) {
        res.err = git::get_last_error();
    }

    return res;
}

struct diff_context_t {
    std::span<const git_oid> current;
    std::vector<git_oid> updated;
    bool contains = false;
};

bool all_oids_zero(const std::vector<git_oid>& ids) {
    return std::ranges::all_of(ids, [](const git_oid& id) { return git_oid_is_zero(&id) != 0; });
}

int collect_delta(const git_diff_delta* delta, float /*unused*/, void* payload) {
    auto* ctx = static_cast<diff_context_t*>(payload);

    for (std::size_t i = 0; i < ctx->current.size(); ++i) {
        // the file was introduced by a later action
        if (git_oid_is_zero(&ctx->current[i]) != 0) {
            continue;
        }

        if (git_oid_equal(&delta->new_file.id, &ctx->current[i]) != 0) {
            ctx->updated[i] = delta->old_file.id;
            ctx->contains   = true;
        }
    }

    return 0;
}

bool iterate_steps(
    git_repository* repo,
    const rebase::RebasePlan& plan,
    const rebase::RebaseAnalysis& result,
    std::function<bool(bool, std::uint32_t, void*)> callback,
    void* payload
) {
    using action::Action;

    const auto* block = result.blocking();
    // no conflict
    if (block == nullptr || block->conflict == nullptr) {
        return true;
    }

    auto* conflict = block->conflict.get();

    std::vector<git_oid> tracked;
    tracked.reserve(conflict->files.size());

    for (const auto& file : conflict->files) {
        if (file.resolved()) {
            continue;
        }

        // not deleted
        if (file.our.has_value()) {
            tracked.push_back(file.our->id);
        }
    }

    git_diff_options diff_opts      = GIT_DIFF_OPTIONS_INIT;
    git_diff_find_options find_opts = GIT_DIFF_FIND_OPTIONS_INIT;
    find_opts.flags                 = GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_RENAMES_FROM_REWRITES;

    git::tree_t block_tree;
    if (git_commit_tree(&block_tree, block->action->get_commit()) != 0) {
        return false;
    }

    git::tree_t root_tree;
    if (git_commit_tree(&root_tree, plan.root_commit()) != 0) {
        return false;
    }

    git_tree* new_tree = block_tree;
    git_tree* old_tree = nullptr;

    for (std::size_t i = block->index; old_tree != root_tree; --i, new_tree = old_tree) {

        if (i > 0) {
            const auto& prev_step = result.steps[i - 1];
            old_tree              = prev_step.tree.get();
        } else {
            old_tree = root_tree;
        }
        diff_context_t ctx {
            .current  = tracked,
            .updated  = tracked,
            .contains = false,

        };

        git::diff_t diff;

        if (git_diff_tree_to_tree(&diff, repo, old_tree, new_tree, &diff_opts) != 0) {
            return false;
        }
        if (git_diff_find_similar(diff, &find_opts) != 0) {
            return false;
        }
        if (git_diff_foreach(diff, collect_delta, nullptr, nullptr, nullptr, &ctx) != 0) {
            return false;
        }

        tracked = std::move(ctx.updated);

        if (!callback(ctx.contains, i, payload)) {
            break;
        }

        if (i == 0 || all_oids_zero(tracked)) {
            return true;
        }
    }

    return true;
}
}
