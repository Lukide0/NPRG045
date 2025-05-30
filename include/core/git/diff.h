#pragma once

#include "core/git/types.h"
#include <git2/diff.h>
#include <git2/oid.h>
#include <git2/types.h>
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

    std::vector<diff_hunk_t> hunks;
};

struct diff_files_header_t {
    diff_file_t new_file;
    diff_file_t old_file;
    diff_files_t::State state;
};

diff_result_t prepare_diff(git_commit* old_commit, git_commit* new_commit, const git_diff_options* opts = nullptr);

std::vector<diff_files_t> create_diff(git_diff* diff);

inline diff_files_header_t diff_header(const diff_files_t& files) {
    return {
        .new_file = files.new_file,
        .old_file = files.old_file,
        .state    = files.state,
    };
}

}
