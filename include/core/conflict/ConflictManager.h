#pragma once

#include "core/conflict/conflict_iterator.h"
#include "core/git/types.h"

#include <map>
#include <span>
#include <string>

#include <git2/index.h>
#include <git2/types.h>

namespace core::conflict {

/**
 * @brief Represents a file-level conflict entry.
 */
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

/**
 * @brief Represents a tree-level conflict between commits.
 */
struct ConflictTrees {
    std::string parent_tree_id;
    std::string commit_id;

    bool operator<(const ConflictTrees& other) const {
        if (parent_tree_id != other.parent_tree_id) {
            return parent_tree_id < other.parent_tree_id;
        }

        return commit_id < other.commit_id;
    }
};

/**
 * @brief Manages conflict resolution for Git operations.
 */
class ConflictManager {
public:
    /**
     * @brief Checks whether an index entry conflict is resolved.
     *
     * @param ancestor Base version entry.
     * @param their Their version entry.
     * @param our Our version entry.
     *
     * @return True if resolved, false otherwise.
     */
    bool is_resolved(const git_index_entry* ancestor, const git_index_entry* their, const git_index_entry* our);

    /**
     * @brief Checks whether a conflict entry is resolved.
     *
     * @param entry Conflict entry.
     *
     * @return True if resolved, false otherwise.
     */
    bool is_resolved(conflict::entry_data_t entry) { return is_resolved(entry.ancestor, entry.their, entry.our); }

    /**
     * @brief Checks whether a conflict entry is resolved.
     *
     * @param entry Conflict entry.
     *
     * @return True if resolved, false otherwise.
     */
    bool is_resolved(const ConflictEntry& entry);

    /**
     * @brief Checks whether all conflicts in an index are resolved.
     *
     * @param index Git index.
     */
    bool is_resolved(git::index_t& index);

    /**
     * @brief Applies stored conflict resolutions.
     *
     * @param entries Conflict entries.
     * @param paths File paths corresponding to entries.
     * @param repo Git repository.
     * @param index Git index.
     *
     * @return True if all resolutions were applied successfully.
     */
    bool apply_resolutions(
        std::span<const ConflictEntry> entries,
        std::span<const std::string> paths,
        git_repository* repo,
        git_index* index
    );

    /**
     * @brief Applies resolutions without writing to index.
     *
     * @param entries Conflict entries.
     * @param paths File paths corresponding to entries.
     * @param repo Git repository.
     * @param index Git index.
     *
     * @return True if successful.
     */
    bool apply_resolutions_no_write(
        std::span<const ConflictEntry> entries,
        std::span<const std::string> paths,
        git_repository* repo,
        git_index* index
    );

    /**
     * @brief Applies a single conflict resolution.
     *
     * @param path File path.
     * @param entry Conflict entry.
     * @param repo Git repository.
     * @param index Git index.
     *
     * @return True if resolution succeeded.
     */
    bool apply_resolution(const std::string& path, const ConflictEntry& entry, git_repository* repo, git_index* index);

    /**
     * @brief Stores a resolved conflict entry.
     *
     * @param entry Conflict entry.
     * @param id Resolution identifier.
     */
    void add_resolution(const ConflictEntry& entry, std::string id);

    /**
     * @brief Stores a resolved tree conflict.
     *
     * @param conflict Tree conflict key.
     * @param resolution Resolved tree object.
     */
    void add_trees_resolution(const ConflictTrees& conflict, core::git::tree_t&& resolution);

    /**
     * @brief Retrieves a resolved tree for a conflict.
     */
    git_tree* get_trees_resolution(const ConflictTrees& conflict);

    /**
     * @brief Retrieves a resolved tree based on old tree and new commit.
     */
    git_tree* get_trees_resolution(const git_tree* old_tree, const git_commit* new_commit);

    /**
     * @brief Gets all conflict entries resolutions.
     */
    [[nodiscard]] const auto& get_conflicts() const { return m_conflicts; }

    /**
     * @brief Gets all tree conflict resolutions.
     */
    [[nodiscard]] const auto& get_tree_conflicts() const { return m_trees; }

    /**
     * @brief Clears all stored conflicts.
     */
    void clear() { m_conflicts.clear(); }

    /**
     * @brief Gets global ConflictManager instance.
     */
    static ConflictManager& get() {
        static ConflictManager manager;
        return manager;
    }

private:
    std::map<ConflictEntry, std::string> m_conflicts;

    std::map<ConflictTrees, core::git::tree_t> m_trees;
};

}
