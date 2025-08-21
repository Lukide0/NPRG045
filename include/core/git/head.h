#pragma once

#include "core/git/types.h"
#include <git2/checkout.h>
#include <git2/oid.h>
#include <git2/refs.h>
#include <git2/repository.h>
#include <git2/types.h>

namespace core::git {

inline bool set_repository_head_detached(git_repository* repo, const git_oid* oid) {
    if (git_repository_set_head_detached(repo, oid) != 0) {
        return false;
    }

    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy |= GIT_CHECKOUT_FORCE;

    return git_checkout_head(repo, &opts) == 0;
}

inline bool set_repository_head(git_repository* repo, const git_reference* ref) {
    const char* name = git_reference_name(ref);

    if (git_repository_set_head(repo, name) != 0) {
        return false;
    }

    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy |= GIT_CHECKOUT_FORCE;

    return git_checkout_head(repo, &opts) == 0;
}

inline bool is_head(git_repository* repo, const git_oid* oid) {
    reference_t head_ref;

    if (git_repository_head(&head_ref, repo) != 0) {
        return false;
    }

    const git_oid* head_oid = git_reference_target(head_ref.get());
    if (head_oid == nullptr) {
        return false;
    }

    return git_oid_equal(oid, head_oid) != 0;
}

}
