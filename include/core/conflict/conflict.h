#pragma once

#include "action/ActionManager.h"
#include "App.h"
#include "core/git/types.h"
#include "core/state/Command.h"

#include <span>
#include <string>
#include <utility>

#include <git2/types.h>

namespace core::conflict {

enum class ConflictStatus {
    ERR,
    HAS_CONFLICT,
    NO_CONFLICT,
};

struct ResolutionResult {
    std::optional<std::string> err;
    git_oid id;
};

std::pair<ConflictStatus, git::index_t> cherrypick_check(git_commit* commit, git_commit* parent_commit);

ResolutionResult add_resolved_files(
    git::index_t& index,
    git_repository* repo,
    std::span<const std::string> paths,
    std::span<const ConflictEntry> entries,
    ConflictManager& manager
);

class ConflictResolveCommand : public core::state::Command {
public:
    ConflictResolveCommand(std::size_t id, git::commit_t&& commit)
        : m_commit(std::move(commit))
        , m_action_id(id) { }

    void undo() override { swap_commits(); }

    void execute() override { swap_commits(); }

private:
    git::commit_t m_commit;
    std::size_t m_action_id;

    void swap_commits() {
        auto& manager = action::ActionsManager::get();

        auto* act = manager.get_action(m_action_id);
        assert(act != nullptr);

        action::ActionsManager::swap_commits(act, m_commit);

        App::updateActions();
    }
};

}
