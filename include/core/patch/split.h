#pragma once

#include "action/Action.h"
#include "core/git/types.h"

namespace core::patch {

bool split(
    core::git::git_commit_t& out_first,
    core::git::git_commit_t& out_second,
    action::Action* act,
    core::git::git_diff_t& patch
);

}
