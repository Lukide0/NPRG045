#pragma once

#include "core/git/types.h"
#include <git2/diff.h>
#include <git2/oid.h>
#include <git2/types.h>
#include <string>
#include <vector>

struct diff_result_t {
    enum State {
        FAILED_TO_RETRIEVE_TREE,
        FAILED_TO_CREATE_DIFF,
        OK,
    };

    State state = State::OK;
    git_diff_t diff;
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
};

struct diff_hunk_t {
    hunk_lines_info new_file;
    hunk_lines_info old_file;

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

diff_result_t prepare_diff(git_commit* old_commit, git_commit* new_commit, const git_diff_options* opts = nullptr);

std::vector<diff_files_t> create_diff(git_diff* diff);
