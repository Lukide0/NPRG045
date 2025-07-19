#pragma once

#include "core/conflict/conflict_iterator.h"
#include "core/git/types.h"

#include <map>
#include <span>
#include <string>

#include <git2/index.h>
#include <git2/types.h>

namespace core::conflict {

struct ConflictEntry {
    std::string ancestor_id;
    std::string their_id;
    std::string our_id;

    bool operator<(const ConflictEntry& other) const {
        if (ancestor_id != other.ancestor_id) {
            return ancestor_id < other.ancestor_id;
        }
        if (their_id != other.their_id) {
            return their_id < other.their_id;
        }
        return our_id < other.our_id;
    }
};

struct ConflictCommits {
    std::string parent_id;
    std::string child_id;

    bool operator<(const ConflictCommits& other) const {
        if (parent_id != other.parent_id) {
            return parent_id < other.parent_id;
        }

        return child_id < other.child_id;
    }
};

class ConflictManager {
public:
    bool is_resolved(const git_index_entry* ancestor, const git_index_entry* their, const git_index_entry* our);

    bool is_resolved(conflict::entry_data_t entry) { return is_resolved(entry.ancestor, entry.their, entry.our); }

    bool is_resolved(const ConflictEntry& entry);

    bool is_resolved(git::index_t& index);

    bool apply_resolutions(
        std::span<const ConflictEntry> entries,
        std::span<const std::string> paths,
        git_repository* repo,
        git_index* index
    );

    bool apply_resolution(const std::string& path, const ConflictEntry& entry, git_repository* repo, git_index* index);

    void add_resolution(const ConflictEntry& entry, std::string id);

    void add_commits_resolution(const ConflictCommits& conflict, core::git::tree_t&& resolution);

    git_tree* get_commits_resolution(const ConflictCommits& conflict);
    git_tree* get_commits_resolution(const git_commit* old_commit, const git_commit* new_commit);

    [[nodiscard]] const auto& get_conflicts() const { return m_conflicts; }

    [[nodiscard]] const auto& get_commits_conflicts() const { return m_commits; }

    void clear() { m_conflicts.clear(); }

    static ConflictManager& get() {
        static ConflictManager manager;
        return manager;
    }

private:
    std::map<ConflictEntry, std::string> m_conflicts;

    std::map<ConflictCommits, core::git::tree_t> m_commits;
};

}
