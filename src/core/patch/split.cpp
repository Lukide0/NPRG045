#include "core/patch/split.h"

#include "core/action/Action.h"
#include "core/action/ActionsManager.h"
#include "core/git/error.h"
#include "core/git/RepoSession.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebaseSession.h"

#include <cassert>

#include <git2/apply.h>
#include <git2/commit.h>
#include <git2/diff.h>
#include <git2/index.h>
#include <git2/oid.h>
#include <git2/tree.h>
#include <git2/types.h>

namespace core::patch {

using action::Action;

SplitError create_copy_commit(
    git_oid* oid, const git_commit* commit, const git_commit* parent, const git_tree* tree, git_repository* repo
) {
    const auto* author    = git_commit_author(commit);
    const auto* committer = git_commit_committer(commit);
    const auto* msg       = git_commit_message_raw(commit);
    const auto* encoding  = git_commit_message_encoding(commit);

    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    const git_commit* parents[] = { parent };

    if (git_commit_create(oid, repo, nullptr, author, committer, encoding, msg, tree, 1, parents) != 0) {
        return { "Failed to create commit", git::get_last_error() };
    } else {
        return { };
    }
}

SplitError create_copy_commit(
    git_oid* oid,
    git::tree_t& out_tree,
    const git_commit* commit,
    const git_commit* parent,
    git_tree* parent_tree,
    git_diff* diff,
    git_repository* repo
) {

    git::index_t index;
    if (git_apply_to_tree(&index, repo, parent_tree, diff, nullptr) != 0) {
        return { "Failed to apply diff", git::get_last_error() };
    }

    assert(git_index_has_conflicts(index) == 0);

    git_oid tree_oid;
    if (git_index_write_tree_to(&tree_oid, index, repo) != 0) {
        return { "Failed to write tree", git::get_last_error() };
    }

    if (git_tree_lookup(&out_tree, repo, &tree_oid) != 0) {
        return { "Created tree object not found in repository", git::get_last_error() };
    }

    return create_copy_commit(oid, commit, parent, out_tree, repo);
}

SplitError create_copy_commit(
    git_oid* oid,
    git::tree_t& out_tree,
    const git_commit* commit,
    const git_commit* parent,
    git_diff* diff,
    git_repository* repo
) {
    git::tree_t parent_tree;
    if (git_commit_tree(&parent_tree, parent) != 0) {
        return { "Failed to find commit tree", git::get_last_error() };
    }

    return create_copy_commit(oid, out_tree, commit, parent, parent_tree, diff, repo);
}

SplitError split(
    git::commit_t& out_first,
    git::commit_t& out_second,
    const git_commit* commit,
    const git_commit* parent_commit,
    git::diff_t& patch
) {
    git_repository* repo = git_commit_owner(commit);

    git::tree_t patch_tree;
    git_oid patch_commit_oid;

    git::tree_t commit_tree;
    if (git_commit_tree(&commit_tree, commit) != 0) {
        return { "Failed to find commit tree", git::get_last_error() };
    }

    // 1. Create commit only from patch
    SplitError er = create_copy_commit(&patch_commit_oid, patch_tree, commit, parent_commit, patch, repo);
    if (er.has_error()) {
        return er;
    }

    if (git_commit_lookup(&out_first, repo, &patch_commit_oid) != 0) {
        return { "Failed to find commit", git::get_last_error() };
    }

    git_oid delta_commit_oid;
    // 3. Create commit
    er = create_copy_commit(&delta_commit_oid, commit, out_first, commit_tree, repo);
    if (er.has_error()) {
        return er;
    }

    if (git_commit_lookup(&out_second, repo, &delta_commit_oid) != 0) {
        return { "Failed to find commit", git::get_last_error() };
    }

    return { };
}

SplitError split(
    rebase::RebaseSession& rebase,
    git::commit_t& out_first,
    git::commit_t& out_second,
    const rebase::StepResult& step,
    const rebase::StepResult* previous_step,
    git::diff_t& patch
) {
    const Action& original = *step.action;

    const git_commit* commit = original.get_commit();
    git_repository* repo     = rebase.repo().repo();

    const action::Action* parent_act = previous_step != nullptr ? previous_step->action : nullptr;
    if (parent_act == nullptr) {
        return split(out_first, out_second, commit, rebase.plan().root_commit(), patch);
    }

    const git_commit* parent_commit = parent_act->get_commit();
    git::tree_t commit_tree;
    if (git_commit_tree(&commit_tree, commit) != 0) {
        return { "Failed to find commit tree", git::get_last_error() };
    }

    git_tree* parent_tree = previous_step->tree.get();

    git::tree_t patch_tree;
    git_oid patch_commit_oid;

    // 1. Create commit only from patch
    SplitError er = create_copy_commit(&patch_commit_oid, patch_tree, commit, parent_commit, parent_tree, patch, repo);
    if (er.has_error()) {
        return er;
    }

    if (git_commit_lookup(&out_first, repo, &patch_commit_oid) != 0) {
        return { "Failed to find commit", git::get_last_error() };
    }

    git_oid delta_commit_oid;
    // 3. Create commit
    er = create_copy_commit(&delta_commit_oid, commit, out_first, commit_tree, repo);
    if (er.has_error()) {
        return er;
    }

    if (git_commit_lookup(&out_second, repo, &delta_commit_oid) != 0) {
        return { "Failed to find commit", git::get_last_error() };
    }

    return { };
}

}
