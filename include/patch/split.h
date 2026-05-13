#pragma once

#include "action/Action.h"
#include "git/types.h"

namespace patch {

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
bool split(git::commit_t& out_first, git::commit_t& out_second, action::Action* act, git::diff_t& patch);

}
