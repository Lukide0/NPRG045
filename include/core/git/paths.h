#pragma once

#include "core/utils/strings.h"
#include <string>

namespace core::git {

/**
 * @brief Prefix for Git rebase-merge directory.
 */
constexpr auto REBASE_PREFIX = core::utils::comptime_str(".git/rebase-merge/");

/**
 * @brief Path to the Git rebase todo file.
 */
constexpr auto TODO_FILE = REBASE_PREFIX + "git-rebase-todo";

/**
 * @brief Path to the original HEAD file.
 */
constexpr auto HEAD_FILE = REBASE_PREFIX + "orig-head";

/**
 * @brief Path to the onto file used in rebase.
 */
constexpr auto ONTO_FILE = REBASE_PREFIX + "onto";

/**
 * @brief Extracts repository path from a rebase todo file path.
 *
 * @param path Full path to a rebase todo file.
 */
std::string repo_path_from_todo(const std::string& path);

}
