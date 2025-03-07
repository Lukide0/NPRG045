#include "core/git/diff.h"
#include "core/git/types.h"
#include "core/utils/unexpected.h"
#include <git2/commit.h>
#include <git2/diff.h>
#include <git2/tree.h>
#include <git2/types.h>
#include <vector>

// NOTE: This callback is called once per file
int diff_file_callback(const git_diff_delta* delta, float /*unused*/, void* state_raw);

// NOTE: This callback is called once per hunk
int diff_hunk_callback(const git_diff_delta* delta, const git_diff_hunk* hunk, void* state_raw);

// NOTE: This callback is called once per line
int diff_line_callback(
    const git_diff_delta* delta, const git_diff_hunk* hunk, const git_diff_line* line, void* state_raw
);

class diff_state_t {
public:
    std::vector<diff_files_t> files;

    diff_files_t& add_file(const git_diff_file& old_file, const git_diff_file& new_file) {
        return create_file(old_file, new_file);
    }

    diff_hunk_t& add_hunk() {
        auto& file = get_file();
        file.hunks.emplace_back();

        return file.hunks.back();
    }

    diff_files_t& get_file() { return files.back(); }

    diff_hunk_t& get_hunk() { return get_file().hunks.back(); }

private:
    static diff_file_t create_file(const git_diff_file& file) {
        return {
            .path = file.path,
            .id   = file.id,
        };
    }

    diff_files_t& create_file(const git_diff_file& old_file, const git_diff_file& new_file) {
        files.push_back({
            .new_file = create_file(new_file),
            .old_file = create_file(old_file),
            .state    = diff_files_t::State::UNMODIFIED,
            .hunks    = {},
        });

        return files.back();
    }
};

diff_result_t prepare_diff(git_commit* old_commit, git_commit* new_commit, const git_diff_options* opts) {
    git_tree_t old_tree;
    git_tree_t new_tree;

    diff_result_t res;
    git_repository* repo = nullptr;

    if (old_commit != nullptr) {
        if (git_commit_tree(&old_tree.tree, old_commit) != 0) {
            res.state = diff_result_t::FAILED_TO_RETRIEVE_TREE;
            return res;
        } else {
            repo = git_tree_owner(old_tree.tree);
        }
    }

    if (new_commit != nullptr) {
        if (git_commit_tree(&new_tree.tree, new_commit) != 0) {
            res.state = diff_result_t::FAILED_TO_RETRIEVE_TREE;
            return res;
        } else if (repo == nullptr) {
            repo = git_tree_owner(new_tree.tree);
        }
    }

    if (git_diff_tree_to_tree(&res.diff.diff, repo, old_tree.tree, new_tree.tree, opts) != 0) {
        res.state = diff_result_t::FAILED_TO_CREATE_DIFF;
    }

    return res;
}

std::vector<diff_files_t> create_diff(git_diff* diff) {
    git_diff_find_similar(diff, nullptr);

    diff_state_t state;

    git_diff_foreach(diff, diff_file_callback, nullptr, diff_hunk_callback, diff_line_callback, &state);

    return state.files;
}

int diff_file_callback(const git_diff_delta* delta, float /*unused*/, void* state_raw) {
    auto* state = reinterpret_cast<diff_state_t*>(state_raw);

    diff_files_t& file = state->add_file(delta->old_file, delta->new_file);
    switch (delta->status) {
    case GIT_DELTA_UNMODIFIED:
        file.state = diff_files_t::State::UNMODIFIED;
        break;
    case GIT_DELTA_ADDED:
        file.state = diff_files_t::State::ADDED;
        break;
    case GIT_DELTA_DELETED:
        file.state = diff_files_t::State::DELETED;
        break;
    case GIT_DELTA_MODIFIED:
        file.state = diff_files_t::State::MODIFIED;
        break;
    case GIT_DELTA_RENAMED:
        file.state = diff_files_t::State::RENAMED;
        break;
    case GIT_DELTA_COPIED:
        file.state = diff_files_t::State::COPIED;
        break;
    case GIT_DELTA_IGNORED:
        file.state = diff_files_t::State::IGNORED;
        break;
    case GIT_DELTA_UNTRACKED:
        file.state = diff_files_t::State::UNTRACKED;
        break;
    case GIT_DELTA_TYPECHANGE:
        file.state = diff_files_t::State::TYPECHANGE;
        break;
    case GIT_DELTA_UNREADABLE:
        file.state = diff_files_t::State::UNREADABLE;
        break;
    case GIT_DELTA_CONFLICTED:
        file.state = diff_files_t::State::CONFLICTED;
        break;
    }

    return 0;
}

int diff_hunk_callback(const git_diff_delta* /*unused*/, const git_diff_hunk* hunk, void* state_raw) {

    auto* state            = reinterpret_cast<diff_state_t*>(state_raw);
    diff_hunk_t& file_hunk = state->add_hunk();

    file_hunk.new_file = {
        .offset = hunk->new_start,
        .count  = hunk->new_lines,
    };

    file_hunk.old_file = {
        .offset = hunk->old_start,
        .count  = hunk->old_lines,
    };

    return 0;
}

int diff_line_callback(
    const git_diff_delta* /*unused*/, const git_diff_hunk* /*unused*/, const git_diff_line* line, void* state_raw
) {
    auto* state            = reinterpret_cast<diff_state_t*>(state_raw);
    diff_hunk_t& file_hunk = state->get_hunk();

    auto origin = static_cast<git_diff_line_t>(line->origin);

    diff_line_t diff;
    diff.new_lineno = line->new_lineno;
    diff.old_lineno = line->old_lineno;

    switch (origin) {
    case GIT_DIFF_LINE_CONTEXT:
        diff.type = diff_line_t::Type::CONTEXT;
        break;
    case GIT_DIFF_LINE_ADDITION:
        diff.type = diff_line_t::Type::ADDITION;
        break;
    case GIT_DIFF_LINE_DELETION:
        diff.type = diff_line_t::Type::DELETION;
        break;
    case GIT_DIFF_LINE_CONTEXT_EOFNL:
        diff.type = diff_line_t::Type::CONTEXT_NO_NEWLINE;
        break;
    case GIT_DIFF_LINE_ADD_EOFNL:
        diff.type = diff_line_t::Type::ADDITION_NEWLINE;
        break;
    case GIT_DIFF_LINE_DEL_EOFNL:
        diff.type = diff_line_t::Type::DELETION_NEWLINE;
        break;

    case GIT_DIFF_LINE_FILE_HDR:
    case GIT_DIFF_LINE_HUNK_HDR:
    case GIT_DIFF_LINE_BINARY:
        UNEXPECTED("Invalid line origin");
        break;
    }

    file_hunk.lines.push_back(diff);

    return 0;
}
