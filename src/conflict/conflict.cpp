#include "conflict/conflict.h"

#include "core/action/Action.h"
#include "core/action/ActionsManager.h"
#include "conflict/ConflictManager.h"
#include "core/git/error.h"
#include "core/git/types.h"

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

bool iterate_actions(
    action::Action& conflict_action,
    git_repository* repo,
    std::span<const git_oid> files,
    std::function<bool(bool, std::uint32_t, void*)> callback,
    void* payload
) {
    using action::Action;

    std::vector<git_oid> tracked { files.begin(), files.end() };

    git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;

    git_diff_find_options find_opts = GIT_DIFF_FIND_OPTIONS_INIT;
    find_opts.flags                 = GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_RENAMES_FROM_REWRITES;

    Action* action = &conflict_action;

    git::tree_t conflict_tree;
    if (git_commit_tree(&conflict_tree, action->get_commit()) != 0) {
        return false;
    }

    git_tree* new_tree   = conflict_tree;
    auto& actions        = action::ActionsManager::get();
    std::uint32_t act_id = actions.get_action_index(action);

    git::tree_t root_tree;
    while (true) {
        Action* prev_act   = action->get_prev();
        git_tree* old_tree = nullptr;

        if (prev_act == nullptr) {
            if (git_commit_tree(&root_tree, actions.get_root_commit()) != 0) {
                return false;
            }
            old_tree = root_tree;
        } else {
            old_tree = prev_act->get_tree();
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

        if (callback(ctx.contains, act_id, payload)) {
            return true;
        }

        if (prev_act == nullptr || all_oids_zero(tracked)) {
            break;
        }

        assert(act_id > 0);

        action   = prev_act;
        new_tree = old_tree;
        --act_id;
    }

    return true;
}

}
