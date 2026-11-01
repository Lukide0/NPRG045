#pragma once

#include "core/git/types.h"
#include "core/Result.h"

#include <git2/types.h>

#include <optional>
#include <string>

namespace core::git {

struct RebaseInfo {
    std::string head;
    std::string onto;
};

class RepoSession {
public:
    static Result<RepoSession> open(const std::string& path);

    static RepoSession invalid() { return { }; }

    git_repository* repo();
    [[nodiscard]] const std::string& path() const;
    [[nodiscard]] const std::string& comment_string() const;

    [[nodiscard]] Result<std::optional<RebaseInfo>> rebase_info() const;

private:
    RepoSession() = default;
    RepoSession(git::repository_t&& repo, std::string path, std::string comment_str);

    git::repository_t m_repo;
    std::string m_path;
    std::string m_comment_str;
};

}
