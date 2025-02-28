#include "core/git/paths.h"
#include <filesystem>
#include <string>

namespace core::git {

std::string repo_path_from_todo(const std::string& path) {
    using fs_path = std::filesystem::path;

    // path/.git/rebase-merge/git-rebase-todo
    fs_path fullpath = std::filesystem::absolute(fs_path(path));

    // path/.git/rebase-merge
    fullpath = fullpath.parent_path();

    // path/.git
    fullpath = fullpath.parent_path();

    // path
    fullpath = fullpath.parent_path();

    return fullpath.string();
}

}
