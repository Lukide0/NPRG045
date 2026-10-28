#pragma once

#include "core/git/RepoSession.h"
#include "core/rebase/RebasePlan.h"

namespace core::rebase {

class RebaseSession {
public:
    RebaseSession(git::RepoSession repo);

    [[nodiscard]] const git::RepoSession& repo() const;
    [[nodiscard]] const RebasePlan& plan() const;

    [[nodiscard]] RebasePlan& plan();

private:
    git::RepoSession m_repo;
    RebasePlan m_plan;
};
}
