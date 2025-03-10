#pragma once

#include "core/utils/strings.h"
#include <string>

namespace core::git {

using namespace core::utils;

constexpr auto REBASE_PREFIX = comptime_str(".git/rebase-merge/");
constexpr auto TODO_FILE     = REBASE_PREFIX + "git-rebase-todo";
constexpr auto HEAD_FILE     = REBASE_PREFIX + "orig-head";
constexpr auto ONTO_FILE     = REBASE_PREFIX + "onto";

std::string repo_path_from_todo(const std::string& path);

}
