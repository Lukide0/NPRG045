#pragma once

#include "action/Action.h"
#include "action/ActionManager.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/types.h"
#include <filesystem>
#include <git2/types.h>
#include <string>
#include <utility>
#include <vector>

namespace core::state {

struct SaveData {
    std::vector<std::pair<action::Action, std::string>> actions;
    std::vector<std::pair<conflict::ConflictEntry, std::string>> conflicts;
    git::commit_t root;

    std::string head;
    std::string onto;
};

class State {
public:
    static bool save(
        const std::filesystem::path& path,
        const std::filesystem::path& repo,
        const std::string& head,
        const std::string& onto
    );

    static std::optional<SaveData> load(const std::filesystem::path& path, git_repository** repo);
};

}
