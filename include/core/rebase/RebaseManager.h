#pragma once

#include "core/conflict/ConflictManager.h"
#include "core/git/RepoSession.h"
#include "core/rebase/RebasePlan.h"
#include "core/rebase/RebaseSession.h"
#include <memory>
#include <utility>

namespace core::rebase {

class RebaseManager {
public:
    RebaseSession& start(git::RepoSession&& repo, RebasePlan&& plan, conflict::ConflictManager&& conflicts) {
        m_session = std::make_unique<RebaseSession>(std::move(repo), std::move(plan), std::move(conflicts));
        return *m_session;
    }

    [[nodiscard]] RebaseSession* current() { return m_session.get(); }

    [[nodiscard]] const RebaseSession* current() const { return m_session.get(); }

    void close() { m_session.reset(); }

private:
    std::unique_ptr<RebaseSession> m_session;
};
}
