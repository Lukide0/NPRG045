#pragma once

#include "action/Action.h"
#include "core/git/types.h"

namespace core::patch {

bool split(
    core::git::commit_t& out_first, core::git::commit_t& out_second, action::Action* act, core::git::diff_t& patch
);

}
