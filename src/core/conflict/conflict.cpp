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
#include <iostream>
#include <print>
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
        git_error_set(GIT_ERROR_INVALID, "Merge commits are not supported");
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

struct tracked_t {
    std::string path;
    bool removed = false;
    bool tracked = true;
};

struct diff_context_t {
    std::span<const tracked_t> current;
    std::span<tracked_t> updated;
    bool contains;
};

bool has_path(const char* p) { return p != nullptr && *p != '\0'; }

int collect_delta(const git_diff_delta* delta, float /*unused*/, void* payload) {
    auto* ctx = static_cast<diff_context_t*>(payload);

    for (std::size_t i = 0; i < ctx->current.size(); ++i) {
        const auto& cur = ctx->current[i];

        if (!cur.tracked) {
            continue;
        }

        const bool exists
            = has_path(delta->new_file.path) && delta->status != GIT_DELTA_DELETED && cur.path == delta->new_file.path;

        if (exists) {
            ctx->contains           = true;
            ctx->updated[i].removed = false;

            // origin of the file or invalid path to follow
            if (delta->status == GIT_DELTA_ADDED || !has_path(delta->old_file.path)) {
                ctx->updated[i].tracked = false;
            } else {
                ctx->updated[i].path = delta->old_file.path;
            }

        } else if (cur.removed) {
            const bool gone = delta->status == GIT_DELTA_DELETED || delta->status == GIT_DELTA_RENAMED;
            if (gone && has_path(delta->old_file.path) && cur.path == delta->old_file.path) {
                ctx->contains           = true;
                ctx->updated[i].tracked = false;
            }
        }
    }

    return 0;
}

bool diff_trees(
    git_repository* repo, git_tree* old_tree, git_tree* new_tree, git_diff_file_cb callback, diff_context_t& ctx
) {
    git_diff_options diff_opts      = GIT_DIFF_OPTIONS_INIT;
    git_diff_find_options find_opts = GIT_DIFF_FIND_OPTIONS_INIT;
    find_opts.flags                 = GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_RENAMES_FROM_REWRITES;

    git::diff_t diff;
    if (git_diff_tree_to_tree(&diff, repo, old_tree, new_tree, &diff_opts) != 0) {
        return false;
    }

    if (git_diff_find_similar(diff, &find_opts) != 0) {
        return false;
    }

    if (git_diff_foreach(diff, callback, nullptr, nullptr, nullptr, &ctx) != 0) {
        return false;
    }

    return true;
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

    // the first step -> no earlier steps to inspect
    if (block->index == 0) {
        return true;
    }

    auto* conflict = block->conflict.get();

    std::vector<tracked_t> tracked;
    tracked.reserve(block->conflict->files.size());

    for (const auto& file : conflict->files) {
        if (file.resolved()) {
            continue;
        }

        if (file.their.has_value() && !file.their->path.empty()) {
            tracked.push_back(
                {
                    .path    = file.their->path,
                    .removed = false,
                    .tracked = true,
                }
            );

        } else if (file.our.has_value() && !file.our->path.empty()) {
            tracked.push_back(
                {
                    .path    = file.our->path,
                    .removed = true, // missing on their side
                    .tracked = true,
                }
            );
        }
    }

    // nothing to track
    if (tracked.empty()) {
        return true;
    }

    git::tree_t root_tree;
    if (git_commit_tree(&root_tree, plan.root_commit()) != 0) {
        return false;
    }

    git_tree* new_tree = result.steps[block->index - 1].tree.get();
    git_tree* old_tree = nullptr;

    // i = index-1, index-2, ..., 0
    for (std::size_t i = block->index; i-- > 0; new_tree = old_tree) {

        if (i > 0) {
            const auto& prev_step = result.steps[i - 1];
            old_tree              = prev_step.tree.get();
        } else /* i == 0 */ {
            old_tree = root_tree;
        }

        std::vector<tracked_t> next = tracked;
        diff_context_t ctx {
            .current  = tracked,
            .updated  = next,
            .contains = false,
        };

        if (!diff_trees(repo, old_tree, new_tree, collect_delta, ctx)) {
            return false;
        }

        tracked = std::move(next);

        // remove untracked entries
        for (std::size_t j = 0; j < tracked.size();) {
            if (!tracked[j].tracked) {
                tracked[j] = std::move(tracked.back());
                tracked.pop_back();
            } else {
                j += 1;
            }
        }

        if (!callback(ctx.contains, i, payload)) {
            break;
        }

        if (tracked.empty()) {
            break;
        }
    }

    return true;
}
}
