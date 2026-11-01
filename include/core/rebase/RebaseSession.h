#pragma once

#include "core/conflict/ConflictManager.h"
#include "core/git/GitGraph.h"
#include "core/git/RepoSession.h"
#include "core/rebase/RebasePlan.h"
#include <utility>

namespace core::rebase {

class RebaseSession {
public:
    RebaseSession(git::RepoSession&& repo, RebasePlan&& plan, conflict::ConflictManager&& conflicts)
        : m_repo(std::move(repo))
        , m_plan(std::move(plan))
        , m_conflicts(std::move(conflicts)) { }

    RebaseSession(RebaseSession&&)            = delete;
    RebaseSession& operator=(RebaseSession&&) = delete;

    RebaseSession(const RebaseSession&)            = delete;
    RebaseSession& operator=(const RebaseSession&) = delete;

    [[nodiscard]] git::RepoSession& repo() { return m_repo; }

    [[nodiscard]] const git::RepoSession& repo() const { return m_repo; }

    [[nodiscard]] RebasePlan& plan() { return m_plan; }

    [[nodiscard]] const RebasePlan& plan() const { return m_plan; }

    [[nodiscard]] conflict::ConflictManager& conflicts() { return m_conflicts; }

    [[nodiscard]] const conflict::ConflictManager& conflicts() const { return m_conflicts; }

private:
    git::RepoSession m_repo;
    RebasePlan m_plan;
    conflict::ConflictManager m_conflicts;
};

}
