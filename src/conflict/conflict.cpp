#include "conflict/conflict.h"

#include "action/Action.h"
#include "action/ActionManager.h"
#include "conflict/ConflictManager.h"
#include "git/error.h"
#include "git/types.h"

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

namespace conflict {

using Result = std::pair<ConflictStatus, git::index_t>;

Result drop_check(git_tree* onto_tree) {
    git::index_t index;

    if (git_index_new(&index) != 0 || git_index_read_tree(index, onto_tree) != 0) {
        return { ConflictStatus::ERR, std::move(index) };
    }

    return { ConflictStatus::NO_CONFLICT, std::move(index) };
}

Result pick_check(git_commit* commit, git_tree* onto_tree) {
    git::index_t index;
    git::tree_t act_tree;
    git::tree_t ancestor_tree;

    if (git_commit_tree(&act_tree, commit) != 0) {
        return { ConflictStatus::ERR, std::move(index) };
    }

    switch (git_commit_parentcount(commit)) {
    case 0:
        break;
    case 1: {
        git::commit_t ancestor_commit;
        if (git_commit_parent(&ancestor_commit, commit, 0) != 0
            || git_commit_tree(&ancestor_tree, ancestor_commit) != 0) {
            return { ConflictStatus::ERR, std::move(index) };
        }
        break;
    }
    // merge commit
    default:
        // set custom error
        git_error_set(GIT_ERROR_INVALID, "Merge commit are not supported");
        return { ConflictStatus::ERR, std::move(index) };
    }

    if (git_merge_trees(&index, git_commit_owner(commit), ancestor_tree, onto_tree, act_tree, nullptr) != 0) {
        return { ConflictStatus::ERR, std::move(index) };
    }

    ConflictStatus status
        = git_index_has_conflicts(index) != 0 ? ConflictStatus::HAS_CONFLICT : ConflictStatus::NO_CONFLICT;

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

Result cherrypick_check(git_commit* commit, git_commit* parent_commit) {
    git::tree_t tree;
    if (git_commit_tree(&tree, parent_commit) != 0) {
        return { ConflictStatus::ERR, git::index_t { } };
    }
    return pick_check(commit, tree);
}

Result cherrypick_check(action::Action* act, git_commit* parent_commit) {
    git::tree_t tree;
    if (git_commit_tree(&tree, parent_commit) != 0) {
        return { ConflictStatus::ERR, git::index_t { } };
    }
    return action_check(act, tree);
}

Result cherrypick_check(action::Action* act, action::Action* parent_act) {
    switch (parent_act->get_tree_status()) {
    case ConflictStatus::UNKNOWN:
    case ConflictStatus::ERR:
    case ConflictStatus::HAS_CONFLICT:
        return { ConflictStatus::UNKNOWN, git::index_t { } };
    case ConflictStatus::NO_CONFLICT:
    case ConflictStatus::RESOLVED_CONFLICT:
        break;
    }

    git_tree* parent_tree = parent_act->get_tree();
    if (parent_tree == nullptr) {
        return { ConflictStatus::ERR, git::index_t { } };
    }
    return action_check(act, parent_tree);
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
    void* payload;
    std::uint32_t act_id;
    git_oid* files;
    std::size_t files_count;
    std::function<bool(bool, std::uint32_t, void*)> callback;
};

constexpr int REQUESTED_STOP = 250;

int iterate_actions_diff(const git_diff_delta* delta, float /*unused*/, void* payload) {
    auto* ctx = static_cast<diff_context_t*>(payload);

    bool contains = false;
    for (std::size_t i = 0; i < ctx->files_count; ++i) {
        git_oid& file = ctx->files[i];

        // deleted file
        if (git_oid_is_zero(&file) != 0) {
            continue;
        }

        if (git_oid_equal(&delta->new_file.id, &file) != 0) {
            // update file oid
            git_oid_cpy(&file, &delta->old_file.id);

            contains = true;
        }
    }

    if (ctx->callback(contains, ctx->act_id, ctx->payload)) {
        return REQUESTED_STOP;
    }

    return 0;
}

int iterate_actions_single(
    diff_context_t& ctx,
    git::diff_t& diff,
    git_repository* repo,
    git_tree* old_tree,
    git_tree* new_tree,
    git_diff_options* diff_opts,
    git_diff_find_options* find_opts
) {
    int status = git_diff_tree_to_tree(&diff, repo, old_tree, new_tree, diff_opts);

    if (status != 0) {
        return status;
    }

    git_diff_find_similar(diff, find_opts);

    return git_diff_foreach(diff, iterate_actions_diff, nullptr, nullptr, nullptr, &ctx);
}

bool iterate_actions(
    action::Action& conflict_action,
    git_repository* repo,
    std::span<const git_oid> files,
    std::function<bool(bool, std::uint32_t, void*)> callback,
    void* payload
) {
    using action::Action;

    // prepare oids
    std::vector<git_oid> file_ids;
    for (auto&& file : files) {
        file_ids.push_back(file);
    }

    git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;

    git_diff_find_options find_opts = GIT_DIFF_FIND_OPTIONS_INIT;
    find_opts.flags                 = GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_RENAMES_FROM_REWRITES;

    Action* action = &conflict_action;

    git::tree_t conflict_tree;
    git_commit* conflict_commit = action->get_commit();

    if (git_commit_tree(&conflict_tree, conflict_commit) != 0) {
        return false;
    }

    git_tree* new_tree = conflict_tree;

    diff_context_t ctx {
        .payload     = payload,
        .act_id      = action::ActionsManager::get().get_action_index(action),
        .files       = file_ids.data(),
        .files_count = file_ids.size(),
        .callback    = callback,
    };

    do {
        git::diff_t diff;
        Action* prev_act = action->get_prev();

        if (prev_act == nullptr) {
            git::tree_t tree;
            git_commit* commit = action::ActionsManager::get().get_root_commit();

            if (git_commit_tree(&tree, commit) != 0) {
                return false;
            }

            int status = iterate_actions_single(ctx, diff, repo, tree, new_tree, &diff_opts, &find_opts);
            if (status == REQUESTED_STOP || status == 0) {
                break;
            } else {
                return false;
            }
        }

        git_tree* old_tree = prev_act->get_tree();

        int status = iterate_actions_single(ctx, diff, repo, old_tree, new_tree, &diff_opts, &find_opts);
        if (status == REQUESTED_STOP) {
            break;
        } else if (status != 0) {
            return false;
        }

        // update action
        action   = prev_act;
        new_tree = old_tree;
        ctx.act_id -= 1;

    } while (true);

    return true;
}

}
