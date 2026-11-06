#include "core/rebase/RebaseAnalyzer.h"
#include "core/conflict/conflict.h"
#include "core/conflict/conflict_iterator.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/error.h"
#include "core/git/RepoSession.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebasePlan.h"
#include "core/Result.h"
#include <algorithm>
#include <expected>
#include <git2/commit.h>
#include <git2/index.h>
#include <git2/oid.h>
#include <git2/tree.h>
#include <git2/types.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace core::rebase {

Result<git::tree_t> write_tree(git_repository* repo, git_index* index) {
    git_oid oid;
    if (git_index_write_tree_to(&oid, index, repo) != 0) {
        return std::unexpected<Error>({
            .title   = "Failed to write tree",
            .message = git::get_last_error(),
        });
    }

    git::tree_t tree;
    if (git_tree_lookup(&tree, repo, &oid) != 0) {
        return std::unexpected<Error>({
            .title   = "Failed to find tree",
            .message = git::get_last_error(),
        });
    }

    return { std::move(tree) };
}

bool resolutions_match(const conflict::ConflictManager& manager, const StepConflict& conflict) {
    return std::ranges::all_of(conflict.files, [&](const ConflictFile& file) -> bool {
        const std::string* current = manager.get_resolution(file.entry);

        if (current == nullptr) {
            if (file.resolution.has_value()) {
                return false;
            }
        } else if (!file.resolution.has_value() || *file.resolution != *current) {
            return false;
        }

        return true;
    });
}

RebaseAnalyzer::RebaseAnalyzer(git::RepoSession& repo, const RebasePlan& plan, conflict::ConflictManager& conflicts)
    : m_repo(repo)
    , m_plan(plan)
    , m_conflicts(conflicts) { }

Result<RebaseAnalysisPtr> RebaseAnalyzer::analyze(const RebaseAnalysis* previous) const {
    const git_commit* root = m_plan.root_commit();
    if (root == nullptr) {
        return std::unexpected<Error>({
            .title   = "Analysis error",
            .message = "The rebase has no root commit",
        });
    }

    RebaseAnalysisPtr result  = std::make_shared<RebaseAnalysis>();
    result->plan_revision     = m_plan.revision();
    result->conflict_revision = m_conflicts.revision();

    git::tree_t root_tree;
    if (git_commit_tree(&root_tree, root) != 0) {
        return std::unexpected<Error>({
            .title   = "Failed to read the root commit tree",
            .message = git::get_last_error(),
        });
    }

    std::optional<std::size_t> first_changed;
    if (previous == nullptr) {
        first_changed = 0;
    }

    result->root_tree    = git::make_shared_ptr_object<git::tree_t>(std::move(root_tree));
    result->root_tree_id = *git_tree_id(result->root_tree.get());

    shared_git_tree prev = result->root_tree;

    for (const action::Action& act : m_plan) {
        StepResult step;
        step.index        = result->steps.size();
        step.input.commit = act.get_oid();
        step.input.drop   = act.get_type() == action::ActionType::DROP;
        step.action       = &act;

        if (prev == nullptr) {
            step.status = conflict::ConflictState::Unknown;
        } else {
            step.has_parent_tree = true;
            step.parent_tree_id  = *git_tree_id(prev.get());

            if (step.input.drop) {
                step.status = conflict::ConflictState::Clean;
                step.tree   = prev;
            } else {
                const StepResult* old = try_use(previous, step);
                if (old != nullptr) {
                    step.status   = old->status;
                    step.conflict = old->conflict;
                    step.tree     = old->tree;
                } else {
                    auto res = compute_pick(act, prev.get(), step);
                    if (!res.has_value()) {
                        step.error  = res.error();
                        step.status = conflict::ConflictState::Error;
                    }
                }
            }
        }

        if (step.status == conflict::ConflictState::Conflict) {
            assert(!result->blocking_index.has_value());
            result->blocking_index = step.index;
        }

        if (!first_changed.has_value()) {
            if (step.index >= previous->steps.size() || !step.equivalent(previous->steps[step.index])) {
                first_changed = step.index;
            }
        }

        prev = step.has_result() ? step.tree : nullptr;
        result->steps.push_back(std::move(step));
    }

    if (!first_changed.has_value()) {
        result->first_changed = std::min(result->steps.size(), previous->steps.size());
    } else {
        result->first_changed = *first_changed;
    }

    try_compute_conflict_impacts(result.get());

    return result;
}

void RebaseAnalyzer::try_compute_conflict_impacts(RebaseAnalysis* result) const {
    if (result == nullptr) {
        return;
    }

    auto step_callback = [](bool contains_file, std::size_t step_index, void* payload) -> bool {
        auto* analysis = static_cast<RebaseAnalysis*>(payload);

        analysis->steps[step_index].extra.affects_later_conflict = contains_file;
        return true;
    };

    conflict::iterate_steps(m_repo.repo(), m_plan, *result, step_callback, result);
}

[[nodiscard]] bool RebaseAnalyzer::is_current(const RebaseAnalysis& analysis) const {
    return analysis.plan_revision == m_plan.revision() && analysis.conflict_revision == m_conflicts.revision();
}

const StepResult* RebaseAnalyzer::try_use(const RebaseAnalysis* previous, const StepResult& step) const {
    if (previous == nullptr) {
        return nullptr;
    }

    const StepResult* old_result = nullptr;
    if (step.index < previous->steps.size()
        && git_oid_equal(&previous->steps[step.index].input.commit, &step.input.commit) != 0) {
        old_result = &previous->steps[step.index];
    } else {
        old_result = previous->find(step.input.commit);
    }

    const bool are_same
        = (old_result != nullptr && old_result->input == step.input && old_result->has_parent_tree
           && git_oid_equal(&old_result->parent_tree_id, &step.parent_tree_id) != 0);

    if (!are_same) {
        return nullptr;
    }

    const auto* old_tree = old_result->tree.get();

    switch (old_result->status) {
    case conflict::ConflictState::Unknown:
    case conflict::ConflictState::Error:
        return nullptr;
    case conflict::ConflictState::Conflict:
        if (old_result->conflict == nullptr || !resolutions_match(m_conflicts, *old_result->conflict)) {
            return nullptr;
        }
        return old_result;

    case conflict::ConflictState::Clean:
        return old_tree != nullptr ? old_result : nullptr;

    case conflict::ConflictState::Resolved:
        if (old_tree == nullptr || old_result->conflict == nullptr
            || !resolutions_match(m_conflicts, *old_result->conflict)) {
            return nullptr;
        }
        return old_result;
    }

    std::unreachable();
}

Result<void> RebaseAnalyzer::compute_pick(const action::Action& act, const git_tree* prev, StepResult& step) const {
    auto [status, merge_index] = conflict::pick_check(act.get_commit(), prev);

    switch (status) {
    case conflict::ConflictState::Unknown:
    case conflict::ConflictState::Resolved:
        step.status = conflict::ConflictState::Error;
        return std::unexpected<Error>({
            .title   = "Conflict update error",
            .message = "unexpected status from the conflict check",
        });

    case conflict::ConflictState::Error:
        step.status = conflict::ConflictState::Error;
        return std::unexpected<Error>({
            .title   = "Conflict update error",
            .message = git::get_last_error(),
        });

    case conflict::ConflictState::Conflict:
        break;

    case conflict::ConflictState::Clean: {
        auto tree = write_tree(m_repo.repo(), merge_index);
        if (!tree.has_value()) {
            step.status = conflict::ConflictState::Error;
            return std::unexpected<Error>(tree.error());
        }

        step.status = conflict::ConflictState::Clean;
        step.tree   = git::make_shared_ptr_object<git::tree_t>(std::move(tree.value()));
        return { };
    }
    }

    // handle conflict
    auto info = std::make_shared<StepConflict>();

    bool iterated = conflict::iterate(merge_index, [&](conflict::entry_data_t entry) -> bool {
        ConflictFile file;
        const char* path = nullptr;

        if (entry.our != nullptr) {
            file.entry.our_id = git_oid_tostr_s(&entry.our->id);
            file.our          = *entry.our;
            path              = entry.our->path;
        }

        if (entry.their != nullptr) {
            file.entry.their_id = git_oid_tostr_s(&entry.their->id);
            file.their          = *entry.their;

            if (path == nullptr) {
                path = entry.their->path;
            }
        }

        if (entry.ancestor != nullptr) {
            file.entry.ancestor_id = git_oid_tostr_s(&entry.ancestor->id);
            file.ancestor          = *entry.ancestor;

            if (path == nullptr) {
                path = entry.ancestor->path;
            }
        }

        for (auto* side : { &file.ancestor, &file.our, &file.their }) {
            if (side->has_value()) {
                (*side)->path = nullptr;
            }
        }

        file.path = path != nullptr ? path : "";

        // lookup saved resolution
        const std::string* resolution = m_conflicts.get_resolution(file.entry);
        if (resolution != nullptr) {
            file.resolution = *resolution;
        }

        info->files.push_back(std::move(file));
        return true;
    });

    if (!iterated) {
        return std::unexpected<Error>({
            .title   = "Conflict iterator error",
            .message = git::get_last_error(),
        });
    }

    if (info->files.empty()) {
        return std::unexpected<Error>({
            .title   = "Conflict update error",
            .message = "the merge reported a conflict without any conflicted entry",
        });
    }

    step.conflict = info;

    if (!info->all_resolved()) {
        step.status = conflict::ConflictState::Conflict;
        return { };
    }

    // collect conflict files
    std::vector<conflict::ConflictEntry> entries;
    std::vector<std::string> paths;

    for (const auto& file : info->files) {
        entries.push_back(file.entry);
        paths.push_back(file.path);
    }

    if (!m_conflicts.apply_resolutions_no_write(entries, paths, m_repo.repo(), merge_index)) {
        return std::unexpected<Error>({
            .title   = "Recorded resolution error",
            .message = git::get_last_error(),
        });
    }

    auto tree = write_tree(m_repo.repo(), merge_index);
    if (!tree.has_value()) {
        return std::unexpected<Error>({
            .title   = "Recorded resolution error",
            .message = tree.error().message,
        });
    }

    step.status = conflict::ConflictState::Resolved;
    step.tree   = git::make_shared_ptr_object<git::tree_t>(std::move(tree.value()));

    return { };
}
}
