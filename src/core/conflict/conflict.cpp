#include "core/conflict/conflict.h"
#include "action/Action.h"
#include "core/conflict/conflict_iterator.h"
#include "core/git/error.h"
#include "core/git/types.h"
#include <git2/blob.h>
#include <git2/cherrypick.h>
#include <git2/commit.h>
#include <git2/index.h>
#include <git2/repository.h>
#include <git2/status.h>
#include <git2/types.h>
#include <memory>
#include <span>
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

std::pair<ConflictStatus, git::index_t> cherrypick_check(action::Action* act, git_commit* parent_commit) {
    git::index_t index;
    git::tree_t tree;

    switch (act->get_type()) {
    case action::ActionType::PICK:
    case action::ActionType::REWORD:
    case action::ActionType::EDIT:
    case action::ActionType::SQUASH:
    case action::ActionType::FIXUP:
        return cherrypick_check(act->get_commit(), parent_commit);

    case action::ActionType::DROP:
        break;
    }

    // use parent commit tree
    if (git_index_new(&index) != 0 || git_commit_tree(&tree, parent_commit) != 0) {
        return std::make_pair(ConflictStatus::ERR, std::move(index));
    }

    if (git_index_read_tree(index, tree) != 0) {
        return std::make_pair(ConflictStatus::ERR, std::move(index));
    }

    return std::make_pair(ConflictStatus::NO_CONFLICT, std::move(index));
}

std::pair<ConflictStatus, git::index_t> cherrypick_check(action::Action* act, action::Action* parent_act) {
    git::index_t index;

    switch (parent_act->get_tree_status()) {
    case ConflictStatus::UNKNOWN:
    case ConflictStatus::ERR:
    case ConflictStatus::HAS_CONFLICT:
        return std::make_pair(ConflictStatus::UNKNOWN, std::move(index));
    case ConflictStatus::NO_CONFLICT:
    case ConflictStatus::RESOLVED_CONFLICT:
        break;
    }

    git_tree* parent_tree = parent_act->get_tree();
    core::git::tree_t act_tree;
    core::git::tree_t ancestor_tree;

    core::git::commit_t parent_commit;
    git_repository* repo = git_commit_owner(act->get_commit());

    if (git_commit_parent(&parent_commit, act->get_commit(), 0) != 0) {
        return std::make_pair(ConflictStatus::ERR, std::move(index));
    }

    if (git_commit_tree(&ancestor_tree, parent_commit) != 0) {
        return std::make_pair(ConflictStatus::ERR, std::move(index));
    }

    if (git_commit_tree(&act_tree, act->get_commit()) != 0) {
        return std::make_pair(ConflictStatus::ERR, std::move(index));
    }

    if (git_merge_trees(&index, repo, ancestor_tree, parent_tree, act_tree, nullptr) != 0) {
        return std::make_pair(ConflictStatus::ERR, std::move(index));
    }

    if (git_index_has_conflicts(index) != 0) {
        return std::make_pair(ConflictStatus::HAS_CONFLICT, std::move(index));
    } else {
        return std::make_pair(ConflictStatus::NO_CONFLICT, std::move(index));
    }
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
            res.err = "Not staged changes";
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

}
