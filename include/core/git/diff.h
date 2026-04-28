#pragma once

#include "action/Action.h"
#include "core/git/types.h"
#include <cstdint>
#include <git2/diff.h>
#include <git2/oid.h>
#include <git2/types.h>
#include <optional>
#include <string>
#include <vector>

namespace core::git {

/**
 * @brief Result of a diff preparation operation.
 */
struct diff_result_t {
    enum State {
        FAILED_TO_RETRIEVE_TREE,
        FAILED_TO_CREATE_DIFF,
        OK,
    };

    State state = State::OK;
    diff_t diff;
};

/**
 * @brief Represents a file involved in a diff.
 */
struct diff_file_t {
    std::string path;
    git_oid id;
};

/**
 * @brief Represents line range information in a hunk.
 */
struct hunk_lines_info {
    int offset;
    int count;
};

/**
 * @brief Represents a single line in a diff hunk.
 */
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

/**
 * @brief Represents a diff hunk.
 */
struct diff_hunk_t {
    hunk_lines_info new_file;
    hunk_lines_info old_file;

    std::string header_context;

    std::vector<diff_line_t> lines;
};

/**
 * @brief Represents a full file diff.
 */
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

/**
 * @brief Lightweight diff file header representation.
 */
struct diff_files_header_t {
    diff_file_t new_file;
    diff_file_t old_file;
    diff_files_t::State state;
    std::uint16_t similarity;
};

/**
 * @brief Represents a conflict diff result.
 */
struct conflict_diff_t {
    std::string diff;
    bool deleted_file = false;
};

/**
 * @brief Creates a diff between two trees.
 *
 * @param old_tree Base tree.
 * @param new_tree Target tree.
 * @param repo Git repository.
 * @param opts Diff options (optional).
 */
diff_result_t
prepare_diff(git_tree* old_tree, git_tree* new_tree, git_repository* repo, const git_diff_options* opts = nullptr);

/**
 * @brief Creates a diff between two commits.
 *
 * @param old_commit Base commit.
 * @param new_commit Target commit.
 * @param opts Diff options (optional).
 */
diff_result_t prepare_diff(git_commit* old_commit, git_commit* new_commit, const git_diff_options* opts = nullptr);

/**
 * @brief Creates a resolution diff between tree and commit.
 *
 * @param old_tree Base tree.
 * @param new_commit Target commit.
 * @param opts Diff options (optional).
 */
diff_result_t
prepare_resolution_diff(git_tree* old_tree, git_commit* new_commit, const git_diff_options* opts = nullptr);

/**
 * @brief Creates a resolution diff from an action.
 *
 * @param old_tree Base tree.
 * @param new_tree Action representing target state.
 * @param repo Git repository.
 * @param opts Diff options (optional).
 */
diff_result_t prepare_resolution_diff(
    git_tree* old_tree, action::Action* new_tree, git_repository* repo, const git_diff_options* opts = nullptr
);

/**
 * @brief Converts raw git diff into structured file diffs.
 *
 * @param diff Git diff object.
 *
 * @return Parsed diff files.
 */
std::vector<diff_files_t> create_diff(git_diff* diff);

/**
 * @brief Extracts header information from a file diff.
 *
 * @param files Diff file.
 *
 * @return Header summary.
 */
inline diff_files_header_t diff_header(const diff_files_t& files) {
    return {
        .new_file   = files.new_file,
        .old_file   = files.old_file,
        .state      = files.state,
        .similarity = files.similarity,
    };
}

/**
 * @brief Creates a conflict diff from index entries.
 *
 * @param repo Git repository.
 * @param ancestor Base entry.
 * @param ours Our entry.
 * @param theirs Their entry.
 *
 * @return Optional conflict diff result.
 */
std::optional<conflict_diff_t> create_conflict_diff(
    git_repository* repo, const git_index_entry* ancestor, const git_index_entry* ours, const git_index_entry* theirs
);

}
