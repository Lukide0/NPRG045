#include "core/patch/split.h"
#include "action/Action.h"
#include "core/git/types.h"

#include <git2/apply.h>
#include <git2/commit.h>
#include <git2/index.h>
#include <git2/oid.h>
#include <git2/tree.h>
#include <git2/types.h>

namespace core::patch {

using action::Action;
using git::git_commit_t;
using git::git_diff_t;
using git::git_index_t;
using git::git_tree_t;

bool create_copy_commit(git_oid* oid, git_commit* commit, git_commit* parent, git_tree* tree, git_repository* repo) {
    const auto* author    = git_commit_author(commit);
    const auto* committer = git_commit_committer(commit);
    const auto* msg       = git_commit_message_raw(commit);
    const auto* encoding  = git_commit_message_encoding(commit);

    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    const git_commit* parents[] = { parent };

    return git_commit_create(oid, repo, nullptr, author, committer, encoding, msg, tree, 1, parents) == 0;
}

bool create_copy_commit(
    git_oid* oid, git_tree_t& out_tree, git_commit* commit, git_commit* parent, git_diff* diff, git_repository* repo
) {
    git_tree_t tree;
    if (git_commit_tree(&tree.tree, parent) != 0) {
        assert(false);
        return false;
    }

    git_index_t index;
    if (git_apply_to_tree(&index.index, repo, tree.tree, diff, nullptr) != 0) {
        assert(false);
        return false;
    }
    assert(git_index_has_conflicts(index.index) == 0);

    git_oid tree_oid;
    if (git_index_write_tree_to(&tree_oid, index.index, repo) != 0) {
        assert(false);
        return false;
    }

    if (git_tree_lookup(&out_tree.tree, repo, &tree_oid) != 0) {
        assert(false);
        return false;
    }

    return create_copy_commit(oid, commit, parent, out_tree.tree, repo);
}

bool split(git_commit_t& out_first, git_commit_t& out_second, Action* act, git_diff_t& patch) {
    auto* commit = act->get_commit();
    auto* repo   = git_commit_owner(commit);

    Action* parent_act = act->get_prev();
    assert(parent_act != nullptr);

    auto* parent_commit = parent_act->get_commit();

    git_tree_t patch_tree;
    git_oid patch_commit_oid;

    git_tree_t commit_tree;
    if (git_commit_tree(&commit_tree.tree, commit) != 0) {
        assert(false);
        return false;
    }

    // 1. Create commit only from patch
    if (!create_copy_commit(&patch_commit_oid, patch_tree, commit, parent_commit, patch.diff, repo)) {
        assert(false);
        return false;
    }

    if (git_commit_lookup(&out_first.commit, repo, &patch_commit_oid) != 0) {
        assert(false);
        return false;
    }

    git_oid delta_commit_oid;
    // 3. Create commit
    if (!create_copy_commit(&delta_commit_oid, commit, out_first.commit, commit_tree.tree, repo)) {
        assert(false);
        return false;
    }

    if (git_commit_lookup(&out_second.commit, repo, &delta_commit_oid) != 0) {
        assert(false);
        return false;
    }

    return true;
}

}
