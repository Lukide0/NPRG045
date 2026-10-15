#include "git/comment.h"
#include "git/error.h"
#include <git2/config.h>
#include <git2/repository.h>
#include <git2/types.h>
#include <string>

namespace git {

struct BestMatch {
    std::string value;
    int level = -1;
};

int config_comment_cb(const git_config_entry* entry, void* payload) {
    auto* match = static_cast<BestMatch*>(payload);
    auto level  = static_cast<int>(entry->level);
    if (level >= match->level && entry->value != nullptr) {
        match->level = level;
        match->value = entry->value;
    }

    return 0;
}

CommentResult get_comment_string(git_repository* repo) {
    git_config* cfg = nullptr;
    if (git_repository_config_snapshot(&cfg, repo) < 0) {
        return { get_last_error(), CommentResult::State::FailedOpenConfig };
    }

    BestMatch match;
    if (git_config_foreach_match(cfg, "^core\\.comment(char|string)$", config_comment_cb, &match) < 0) {
        git_config_free(cfg);
        return { get_last_error(), CommentResult::State::FailedReadConfig };
    }

    git_config_free(cfg);

    if (match.value.empty()) {
        if (match.level == -1) {
            return { "#", CommentResult::State::Ok };
        } else {
            return { "core.commentChar|commentString must have at least one character", CommentResult::State::Empty };
        }
    }

    if (match.value == "auto") {
        return { "core.commentChar|commentString=auto is deprecated and not supported",
                 CommentResult::State::Deprecated };
    }

    return { match.value, CommentResult::State::Ok };
}

}
