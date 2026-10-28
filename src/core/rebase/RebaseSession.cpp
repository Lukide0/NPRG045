#include "core/rebase/RebaseSession.h"
#include "core/git/RepoSession.h"
#include "core/rebase/RebasePlan.h"
#include <utility>

namespace core::rebase {

RebaseSession::RebaseSession(git::RepoSession repo)
    : m_repo(std::move(repo)) { }

}
