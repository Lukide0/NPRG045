#pragma once

#include "action/Action.h"
#include "core/git/types.h"

namespace core::patch {

/**
 * @brief Splits a commit into two parts using a patch.
 *
 * @param out_first Resulting first commit.
 * @param out_second Resulting second commit.
 * @param act Action associated with the split.
 * @param patch Diff patch used for splitting.
 *
 * @return True if split succeeded.
 */
bool split(
    core::git::commit_t& out_first, core::git::commit_t& out_second, action::Action* act, core::git::diff_t& patch
);

}
