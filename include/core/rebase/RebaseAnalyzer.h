#pragma once

#include "core/action/Action.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/RepoSession.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebasePlan.h"
#include "core/Result.h"

namespace core::rebase {

class RebaseAnalyzer {
public:
    RebaseAnalyzer(git::RepoSession& repo, const RebasePlan& plan, conflict::ConflictManager& conflicts);

    ~RebaseAnalyzer() = default;

    RebaseAnalyzer(const RebaseAnalyzer&)            = delete;
    RebaseAnalyzer& operator=(const RebaseAnalyzer&) = delete;

    RebaseAnalyzer(RebaseAnalyzer&&)            = delete;
    RebaseAnalyzer& operator=(RebaseAnalyzer&&) = delete;

    Result<RebaseAnalysisPtr> analyze(const RebaseAnalysis* previous = nullptr) const;
    [[nodiscard]] bool is_current(const RebaseAnalysis& analysis) const;

private:
    git::RepoSession& m_repo;
    const RebasePlan& m_plan;
    conflict::ConflictManager& m_conflicts;

    const StepResult* try_use(const RebaseAnalysis* previous, const StepResult& step) const;
    Result<void> compute_pick(const action::Action& act, const git_tree* prev, StepResult& step) const;
    void try_compute_conflict_impacts(RebaseAnalysis* result) const;
};

}
