#pragma once

#include "core/action/Action.h"
#include "core/conflict/conflict.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/types.h"
#include "core/Result.h"
#include <algorithm>
#include <cstddef>
#include <git2/index.h>
#include <git2/oid.h>
#include <git2/types.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace core::rebase {

using shared_git_tree = git::shared_ptr_object_t<git::tree_t>;

struct StepInput {
    git_oid commit { };
    bool drop = false;

    bool operator==(const StepInput& other) const {
        return drop == other.drop && git_oid_equal(&commit, &other.commit) != 0;
    }
};

struct ConflictFile {
    std::string path;

    std::optional<git_index_entry> ancestor;
    std::optional<git_index_entry> our;
    std::optional<git_index_entry> their;

    std::optional<std::string> resolution;

    conflict::ConflictEntry entry;

    [[nodiscard]] bool resolved() const { return resolution.has_value(); }
};

struct StepConflict {
    std::vector<ConflictFile> files;

    [[nodiscard]] std::size_t unresolved_count() {
        return std::ranges::count_if(files, [](const ConflictFile& file) -> bool { return !file.resolved(); });
    }

    [[nodiscard]] bool all_resolved() const {
        return std::ranges::all_of(files, [](const ConflictFile& file) -> bool { return file.resolved(); });
    }
};

struct StepResult {
    std::size_t index            = 0;
    const action::Action* action = nullptr;

    bool has_parent_tree = false;
    git_oid parent_tree_id { };

    shared_git_tree tree;
    conflict::ConflictState status = conflict::ConflictState::Unknown;

    StepInput input;
    std::shared_ptr<StepConflict> conflict;
    std::optional<Error> error;

    // extra data which is excluded from comparison
    struct {
        bool affects_later_conflict = false;
    } extra;

    [[nodiscard]] bool has_result() const {
        return status == conflict::ConflictState::Clean || status == conflict::ConflictState::Resolved;
    }

    [[nodiscard]] bool equivalent(const StepResult& other) const {
        return input == other.input && status == other.status && has_parent_tree == other.has_parent_tree
            && git_oid_equal(&parent_tree_id, &other.parent_tree_id) != 0 && same_tree(tree, other.tree)
            && same_conflict(conflict, other.conflict) && same_error(error, other.error);
    }

private:
    static bool same_tree(const shared_git_tree& a, const shared_git_tree& b) {
        const git_tree* x = a.get();
        const git_tree* y = b.get();

        if (x == nullptr || y == nullptr) {
            return x == y;
        }
        return git_oid_equal(git_tree_id(x), git_tree_id(y)) != 0;
    }

    static bool same_conflict(const std::shared_ptr<StepConflict>& a, const std::shared_ptr<const StepConflict>& b) {
        if (a == b) {
            return true;
        }
        if (a == nullptr || b == nullptr || a->files.size() != b->files.size()) {
            return false;
        }

        for (std::size_t i = 0; i < a->files.size(); ++i) {
            const ConflictFile& x = a->files[i];
            const ConflictFile& y = b->files[i];

            if (x.path != y.path || x.entry != y.entry || x.resolution != y.resolution) {
                return false;
            }
        }
        return true;
    }

    static bool same_error(const std::optional<Error>& a, const std::optional<Error>& b) {
        return a.has_value() == b.has_value() && (!a.has_value() || *a == *b);
    }
};

struct RebaseAnalysis {
    std::size_t plan_revision;
    std::size_t first_changed = 0;
    std::vector<StepResult> steps;

    std::optional<std::size_t> blocking_index;

    git_oid root_tree_id { };
    shared_git_tree root_tree;

    [[nodiscard]] const StepResult* blocking() const {
        if (blocking_index.has_value()) {
            return &steps[*blocking_index];
        } else {
            return nullptr;
        }
    }

    [[nodiscard]] const StepResult* find(const git_oid& commit) const {
        for (const auto& step : steps) {
            if (git_oid_equal(&commit, &step.input.commit) != 0) {
                return &step;
            }
        }

        return nullptr;
    }
};

using RebaseAnalysisPtr      = std::shared_ptr<RebaseAnalysis>;
using RebaseAnalysisConstPtr = std::shared_ptr<const RebaseAnalysis>;
}
