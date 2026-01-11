#pragma once

#include "core/git/types.h"
#include <cstdint>
#include <git2/diff.h>
#include <git2/oid.h>
#include <git2/types.h>
#include <optional>
#include <string>
#include <vector>

namespace core::git {

struct diff_result_t {
    enum State {
        FAILED_TO_RETRIEVE_TREE,
        FAILED_TO_CREATE_DIFF,
        OK,
    };

    State state = State::OK;
    diff_t diff;
};

struct diff_file_t {
    std::string path;
    git_oid id;
};

struct hunk_lines_info {
    int offset;
    int count;
};

struct diff_line_t {
    enum class Type {
        CONTEXT,
        CONTEXT_NO_NEWLINE,
        ADDITION,
        ADDITION_NEWLINE,
        DELETION,
        DELETION_NEWLINE,
    };

    Type type;
    int old_lineno;
    int new_lineno;

    std::string content;
};

struct diff_hunk_t {
    hunk_lines_info new_file;
    hunk_lines_info old_file;

    std::string header_context;

    std::vector<diff_line_t> lines;
};

struct diff_files_t {
    enum class State {
        UNMODIFIED,
        ADDED,
        DELETED,
        MODIFIED,
        RENAMED,
        COPIED,
        IGNORED,
        UNTRACKED,
        TYPECHANGE,
        UNREADABLE,
        CONFLICTED,
    };

    diff_file_t new_file;
    diff_file_t old_file;
    State state;
    std::uint16_t similarity;

    std::vector<diff_hunk_t> hunks;

    static constexpr const char* state_to_str(State state) {
        switch (state) {
        case State::UNMODIFIED:
            return "unmodified";
        case State::ADDED:
            return "added";
        case State::DELETED:
            return "deleted";
        case State::MODIFIED:
            return "modified";
        case State::RENAMED:
            return "renamed";
        case State::COPIED:
            return "copied";
        case State::IGNORED:
            return "ignored";
        case State::UNTRACKED:
            return "untracted";
        case State::TYPECHANGE:
            return "typechange";
        case State::UNREADABLE:
            return "unreadable";
        case State::CONFLICTED:
            return "conflicted";
        default:
            return "unknown";
        }
    }
};

struct diff_files_header_t {
    diff_file_t new_file;
    diff_file_t old_file;
    diff_files_t::State state;
    std::uint16_t similarity;
};

diff_result_t
prepare_diff(git_tree* old_tree, git_tree* new_tree, git_repository* repo, const git_diff_options* opts = nullptr);

diff_result_t prepare_diff(git_commit* old_commit, git_commit* new_commit, const git_diff_options* opts = nullptr);

diff_result_t
prepare_resolution_diff(git_tree* old_tree, git_commit* new_commit, const git_diff_options* opts = nullptr);

std::vector<diff_files_t> create_diff(git_diff* diff);

inline diff_files_header_t diff_header(const diff_files_t& files) {
    return {
        .new_file   = files.new_file,
        .old_file   = files.old_file,
        .state      = files.state,
        .similarity = files.similarity,
    };
}

std::optional<std::string> create_conflict_diff(
    git_repository* repo, const git_index_entry* ancestor, const git_index_entry* ours, const git_index_entry* theirs
);

}
