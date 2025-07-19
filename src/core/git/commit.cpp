#include "core/git/commit.h"

#include <cassert>
#include <cstddef>

#include <git2/commit.h>
#include <git2/oid.h>
#include <git2/types.h>

namespace core::git {

bool create_commit(
    git_oid* out_oid,
    git_repository* repo,
    const git_signature* author,
    const git_signature* committer,
    const char* msg,
    const git_tree* tree,
    const git_commit* parent
) {
    assert(parent != nullptr);

    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    const git_commit* parents[] = { parent };

    return create_commit(out_oid, repo, author, committer, msg, tree, parents, 1);
}

bool create_commit(
    git_oid* out_oid,
    git_repository* repo,
    const git_signature* author,
    const git_signature* committer,
    const char* msg,
    const git_tree* tree,
    const git_commit** parents,
    std::size_t parent_count
) {
    assert(author != nullptr && committer != nullptr);
    return git_commit_create(out_oid, repo, nullptr, author, committer, nullptr, msg, tree, parent_count, parents) == 0;
}

bool modify_commit(
    git_oid* out_oid,
    const git_commit* commit,
    const git_signature* committer,
    const git_tree* tree,
    const git_commit* parent
) {
    assert(parent != nullptr);

    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    const git_commit* parents[] = { parent };

    return modify_commit(out_oid, commit, committer, tree, parents, 1);
}

bool modify_commit(
    git_oid* out_oid,
    const git_commit* commit,
    const git_signature* committer,
    const git_tree* tree,
    const git_commit** parents,
    std::size_t parent_count
) {
    const auto* author = git_commit_author(commit);
    const auto* msg    = git_commit_message(commit);
    auto* repo         = git_commit_owner(commit);

    return create_commit(out_oid, repo, author, committer, msg, tree, parents, parent_count);
}

}
