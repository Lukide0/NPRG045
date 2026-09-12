#include "git/commit.h"
#include "git/types.h"

#include <cassert>
#include <cstddef>

#include <git2/buffer.h>
#include <git2/commit.h>
#include <git2/graph.h>
#include <git2/oid.h>
#include <git2/remote.h>
#include <git2/revwalk.h>
#include <git2/types.h>

namespace git {

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

bool iterate_branch_commits(git_repository* repo, const char* branch_name, std::function<void(git_commit*)> commit_cb) {
    revwalk_t walker;

    if (git_revwalk_new(&walker, repo) != 0) {
        return false;
    }

    if (git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME) != 0) {
        return false;
    }

    std::string refname = std::string("refs/heads/") + branch_name;
    if (git_revwalk_push_ref(walker, refname.c_str()) != 0) {
        return false;
    }

    git_oid oid;
    while (git_revwalk_next(&oid, walker) == 0) {
        git_commit* commit = nullptr;

        if (git_commit_lookup(&commit, repo, &oid) != 0) {
            continue;
        }

        commit_cb(commit);

        git_commit_free(commit);
    }

    return true;
}

void try_fetch_commits(
    git_repository* repo, const char* branch_name, git_reference* local_ref, git_reference** out_upstream
) {
    *out_upstream = nullptr;

    buffer_t remote_name { GIT_BUF_INIT };

    if (git_branch_lookup(&local_ref, repo, branch_name, GIT_BRANCH_LOCAL) != 0) {
        return;
    }

    // no upstream
    if (git_branch_upstream(out_upstream, local_ref) != 0) {
        return;
    }

    if (git_branch_remote_name(&remote_name, repo, git_reference_name(*out_upstream)) != 0) {
        return;
    }

    remote_t remote;
    if (git_remote_lookup(&remote, repo, remote_name.get().ptr) != 0) {
        return;
    }

    if (git_remote_fetch(remote, nullptr, nullptr, nullptr) != 0) {
        return;
    }

    git_reference* original_upstream = *out_upstream;
    if (git_branch_upstream(out_upstream, local_ref) == 0) {
        git_reference_free(original_upstream);
    } else {
        *out_upstream = original_upstream;
    }
}

branch_state_t check_branch(git_repository* repo, const char* branch_name) {

    reference_t local_ref;
    reference_t upstream_ref;

    branch_state_t result = {
        .behind = 0,
        .ahead  = 0,
    };

    if (git_branch_lookup(&local_ref, repo, branch_name, GIT_BRANCH_LOCAL) != 0) {
        return result;
    }

    try_fetch_commits(repo, branch_name, local_ref, &upstream_ref);

    // no upstream
    if (upstream_ref == nullptr) {
        return result;
    }

    git_graph_ahead_behind(
        &result.ahead, &result.behind, repo, git_reference_target(local_ref), git_reference_target(upstream_ref)
    );

    return result;
}

}
