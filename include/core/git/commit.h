#pragma once

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
    const git_commit** parents,
    std::size_t parent_count
);

bool create_commit(
    git_oid* out_oid,
    git_repository* repo,
    const git_signature* author,
    const git_signature* committer,
    const char* msg,
    const git_tree* tree,
    const git_commit* parent
);

bool modify_commit(
    git_oid* out_oid,
    const git_commit* commit,
    const git_signature* committer,
    const git_tree* tree,
    const git_commit** parents,
    std::size_t parent_count
);

bool modify_commit(
    git_oid* out_oid,
    const git_commit* commit,
    const git_signature* committer,
    const git_tree* tree,
    const git_commit* parent
);

}
