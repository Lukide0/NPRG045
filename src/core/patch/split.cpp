#include "core/patch/split.h"
#include "action/Action.h"
#include "action/ActionManager.h"
#include "core/git/types.h"

#include <git2/apply.h>
#include <git2/commit.h>
#include <git2/index.h>
#include <git2/oid.h>
#include <git2/tree.h>
#include <git2/types.h>

namespace core::patch {

using action::Action;

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
    git_oid* oid, git::tree_t& out_tree, git_commit* commit, git_commit* parent, git_diff* diff, git_repository* repo
) {
    git::tree_t tree;
    if (git_commit_tree(&tree, parent) != 0) {
        assert(false);
        return false;
    }

    git::index_t index;
    if (git_apply_to_tree(&index, repo, tree, diff, nullptr) != 0) {
        assert(false);
        return false;
    }
    assert(git_index_has_conflicts(index) == 0);

    git_oid tree_oid;
    if (git_index_write_tree_to(&tree_oid, index, repo) != 0) {
        assert(false);
        return false;
    }

    if (git_tree_lookup(&out_tree, repo, &tree_oid) != 0) {
        assert(false);
        return false;
    }

    return create_copy_commit(oid, commit, parent, out_tree, repo);
}

bool split(git::commit_t& out_first, git::commit_t& out_second, Action* act, git::diff_t& patch) {
    auto* commit = act->get_commit();
    auto* repo   = git_commit_owner(commit);

    git_commit* parent_commit = action::ActionsManager::get_parent_commit(act);
    assert(parent_commit != nullptr);

    git::tree_t patch_tree;
    git_oid patch_commit_oid;

    git::tree_t commit_tree;
    if (git_commit_tree(&commit_tree, commit) != 0) {
        assert(false);
        return false;
    }

    // 1. Create commit only from patch
    if (!create_copy_commit(&patch_commit_oid, patch_tree, commit, parent_commit, patch, repo)) {
        assert(false);
        return false;
    }

    if (git_commit_lookup(&out_first, repo, &patch_commit_oid) != 0) {
        assert(false);
        return false;
    }

    git_oid delta_commit_oid;
    // 3. Create commit
    if (!create_copy_commit(&delta_commit_oid, commit, out_first, commit_tree, repo)) {
        assert(false);
        return false;
    }

    if (git_commit_lookup(&out_second, repo, &delta_commit_oid) != 0) {
        assert(false);
        return false;
    }

    return true;
}

}
