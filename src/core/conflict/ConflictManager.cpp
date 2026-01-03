#include "core/conflict/ConflictManager.h"
#include "core/git/types.h"

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <git2/blob.h>
#include <git2/errors.h>
#include <git2/index.h>
#include <git2/oid.h>
#include <git2/sys/errors.h>
#include <git2/types.h>
#include <ios>
#include <span>
#include <string>
#include <utility>

namespace core::conflict {

bool ConflictManager::is_resolved(
    const git_index_entry* ancestor, const git_index_entry* their, const git_index_entry* our
) {
    ConflictEntry entry;

    if (ancestor != nullptr) {
        entry.ancestor_id = git_oid_tostr_s(&ancestor->id);
    }

    if (their != nullptr) {
        entry.their_id = git_oid_tostr_s(&their->id);
    }

    if (our != nullptr) {
        entry.our_id = git_oid_tostr_s(&our->id);
    }

    return is_resolved(entry);
}

bool ConflictManager::is_resolved(const ConflictEntry& entry) { return m_conflicts.contains(entry); }

bool ConflictManager::is_resolved(git::index_t& index) {
    if (git_index_has_conflicts(index.get()) == 0) {
        return true;
    }

    bool resolved = true;

    bool iterator_status = iterate(index.get(), [this, &resolved](entry_data_t entry) -> bool {
        if (!is_resolved(entry)) {
            resolved = false;
            return false;
        }

        return true;
    });

    assert(iterator_status);

    return resolved;
}

bool ConflictManager::apply_resolutions(
    std::span<const ConflictEntry> entries, std::span<const std::string> paths, git_repository* repo, git_index* index
) {
    bool status = apply_resolutions_no_write(entries, paths, repo, index);

    return status && git_index_write(index) == 0;
}

bool ConflictManager::apply_resolutions_no_write(
    std::span<const ConflictEntry> entries, std::span<const std::string> paths, git_repository* repo, git_index* index
) {
    bool status = true;

    assert(entries.size() == paths.size());
    for (std::size_t i = 0; i < entries.size(); ++i) {
        status &= apply_resolution(paths[i], entries[i], repo, index);
    }

    return status;
}

bool ConflictManager::apply_resolution(
    const std::string& path, const ConflictEntry& entry, git_repository* repo, git_index* index
) {
    auto it = m_conflicts.find(entry);

    if (it == m_conflicts.end()) {
        return true;
    }

    const std::string& resolution_id = it->second;

    const git_index_entry* ancestor = nullptr;
    const git_index_entry* our      = nullptr;
    const git_index_entry* their    = nullptr;

    if (git_index_conflict_get(&ancestor, &our, &their, index, path.c_str()) != 0) {
        return false;
    }

    if (git_index_conflict_remove(index, path.c_str()) != 0) {
        return false;
    }

    if (git_index_remove_bypath(index, path.c_str()) != 0) {
        return false;
    }

    // The file is deleted
    if (resolution_id.empty()) {
        return true;
    }

    git_oid oid;
    if (git_oid_fromstr(&oid, resolution_id.c_str()) != 0) {
        return false;
    }

    git::blob_t blob;
    if (git_blob_lookup(&blob, repo, &oid) != 0) {
        return false;
    }

    std::size_t content_size = git_blob_rawsize(blob);

    git_index_entry new_entry;
    std::memset(&new_entry, 0, sizeof(git_index_entry));

    git_oid_cpy(&new_entry.id, &oid);
    new_entry.path           = path.c_str();
    new_entry.file_size      = content_size;
    new_entry.flags          = 0;
    new_entry.flags_extended = 0;

    git_index_time time_now;
    time_now.seconds     = std::time(nullptr);
    time_now.nanoseconds = 0;

    if (our != nullptr) {
        new_entry.mode  = our->mode;
        new_entry.ctime = our->ctime;
        new_entry.mtime = our->mtime;
        new_entry.dev   = our->dev;
        new_entry.ino   = our->ino;
        new_entry.uid   = our->uid;
        new_entry.gid   = our->gid;
    } else if (their != nullptr) {
        new_entry.mode  = their->mode;
        new_entry.ctime = their->ctime;
        new_entry.mtime = their->mtime;
        new_entry.dev   = their->dev;
        new_entry.ino   = their->ino;
        new_entry.uid   = their->uid;
        new_entry.gid   = their->gid;
    } else {
        std::uint32_t filemode = GIT_FILEMODE_BLOB;

        if (ancestor != nullptr) {
            filemode = ancestor->mode;
        }
        new_entry.mode  = filemode;
        new_entry.ctime = time_now;
        new_entry.mtime = time_now;
        new_entry.dev   = 0;
        new_entry.ino   = 0;
        new_entry.uid   = 0;
        new_entry.gid   = 0;
    }

    if (git_index_add(index, &new_entry) != 0) {
        return false;
    }

    return true;
}

void ConflictManager::add_resolution(const ConflictEntry& entry, std::string id) { m_conflicts[entry] = id; }

void ConflictManager::add_trees_resolution(const ConflictTrees& conflict, core::git::tree_t&& resolution) {
    m_trees[conflict] = std::move(resolution);
}

git_tree* ConflictManager::get_trees_resolution(const git_tree* old_tree, const git_commit* new_commit) {
    ConflictTrees conflict;

    conflict.parent_tree_id = git::format_oid_to_str<git::OID_SIZE>(git_tree_id(old_tree));
    conflict.commit_id      = git::format_oid_to_str<git::OID_SIZE>(git_commit_id(new_commit));

    return get_trees_resolution(conflict);
}

git_tree* ConflictManager::get_trees_resolution(const ConflictTrees& conflict) {
    auto it = m_trees.find(conflict);
    if (it != m_trees.end()) {
        return it->second.get();
    }

    return nullptr;
}

}
