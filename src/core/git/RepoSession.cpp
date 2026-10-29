#include "core/git/RepoSession.h"

#include "core/git/comment.h"
#include "core/git/error.h"
#include "core/git/parser.h"
#include "core/git/types.h"
#include "core/Result.h"
#include <expected>
#include <git2/repository.h>
#include <git2/types.h>
#include <optional>
#include <string>
#include <utility>

namespace core::git {

RepoSession::RepoSession(git::repository_t&& repo, std::string path, std::string comment_str)
    : m_repo(std::move(repo))
    , m_path(std::move(path))
    , m_comment_str(std::move(comment_str)) { }

Result<RepoSession> RepoSession::open(const std::string& path) {
    git::repository_t repo;
    if (git_repository_open(&repo, path.c_str()) != 0) {
        return std::unexpected<Error>({
            .title   = "Failed to open repository",
            .message = git::get_last_error(),
        });
    }

    auto comment_res = git::get_comment_string(repo);
    if (!comment_res.has_value()) {
        return std::unexpected<Error>({
            .title   = "Failed to read Git config",
            .message = comment_res.err_message(),
        });
    }

    return RepoSession {
        std::move(repo),
        path,
        comment_res.value(),
    };
}

git_repository* RepoSession::repo() { return m_repo; }

[[nodiscard]] const std::string& RepoSession::path() const { return m_path; }

[[nodiscard]] const std::string& RepoSession::comment_string() const { return m_comment_str; }

Result<std::optional<RebaseInfo>> RepoSession::rebase_info() const {
    RebaseInfo info;

    auto err = git::get_rebase_info(m_path, info.head, info.onto);
    if (err.has_value()) {
        switch (err->type) {
        case git::RebaseInfoError::NotFound:
            return std::nullopt;

        case git::RebaseInfoError::EmptyOrCorrupted:
            return std::unexpected<Error>({
                .title   = "Failed to read rebase files",
                .message = err->msg,
            });
        }

        std::unreachable();
    }

    return info;
}
}
