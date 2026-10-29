#pragma once

#include "core/conflict/conflict.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/types.h"
#include <algorithm>
#include <cstddef>
#include <git2/index.h>
#include <git2/oid.h>
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

    [[nodiscard]] bool all_resolved() const {
        return std::ranges::all_of(files, [](const ConflictFile& file) -> bool { return file.resolved(); });
    }
};

struct StepResult {
    std::size_t index = 0;

    bool has_parent_tree = false;
    git_oid parent_tree_id { };

    shared_git_tree tree;
    conflict::ConflictStatus status = conflict::ConflictStatus::UNKNOWN;

    StepInput input;
    std::shared_ptr<StepConflict> conflict;

    [[nodiscard]] bool has_result() const {
        return status == conflict::ConflictStatus::NO_CONFLICT || status == conflict::ConflictStatus::RESOLVED_CONFLICT;
    }
};

struct RebaseAnalysis {
    std::size_t plan_revision;
    std::vector<StepResult> steps;

    git_oid root_tree_id { };
    shared_git_tree root_tree;

    [[nodiscard]] const StepResult* find(const git_oid& commit) const {
        for (const auto& step : steps) {
            if (git_oid_equal(&commit, &step.input.commit) != 0) {
                return &step;
            }
        }

        return nullptr;
    }
};
}
