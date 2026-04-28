#pragma once

#include <cassert>
#include <cstddef>
#include <git2/commit.h>
#include <git2/oid.h>
#include <git2/types.h>

namespace core::git {

/**
 * @brief Creates a new Git commit.
 *
 * @param out_oid Resulting commit OID.
 * @param repo Git repository.
 * @param author Author signature.
 * @param committer Committer signature.
 * @param msg Commit message.
 * @param tree Tree object.
 * @param parents Parent commits array.
 * @param parent_count Number of parents.
 *
 * @return True if commit creation succeeded.
 */
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

/**
 * @brief Creates a new Git commit with a single parent.
 *
 * @param out_oid Resulting commit OID.
 * @param repo Git repository.
 * @param author Author signature.
 * @param committer Committer signature.
 * @param msg Commit message.
 * @param tree Tree object.
 * @param parent Parent commit.
 *
 * @return True if commit creation succeeded.
 */
bool create_commit(
    git_oid* out_oid,
    git_repository* repo,
    const git_signature* author,
    const git_signature* committer,
    const char* msg,
    const git_tree* tree,
    const git_commit* parent
);

/**
 * @brief Modifies an existing commit.
 *
 * @param out_oid Resulting commit OID.
 * @param commit Commit to modify.
 * @param committer Committer signature.
 * @param tree New tree object.
 * @param parents Parent commits array.
 * @param parent_count Number of parents.
 *
 * @return True if modification succeeded.
 */
bool modify_commit(
    git_oid* out_oid,
    const git_commit* commit,
    const git_signature* committer,
    const git_tree* tree,
    const git_commit** parents,
    std::size_t parent_count
);

/**
 * @brief Modifies an existing commit with a single parent.
 *
 * @param out_oid Resulting commit OID.
 * @param commit Commit to modify.
 * @param committer Committer signature.
 * @param tree New tree object.
 * @param parent Parent commit.
 *
 * @return True if modification succeeded.
 */
bool modify_commit(
    git_oid* out_oid,
    const git_commit* commit,
    const git_signature* committer,
    const git_tree* tree,
    const git_commit* parent
);

}
