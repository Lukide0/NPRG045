#include "core/conflict/ConflictManager.h"
#include "core/git/types.h"

#include <cstddef>
#include <cstdint>
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
    entry.ancestor_id = git_oid_tostr_s(&ancestor->id);
    entry.their_id    = git_oid_tostr_s(&their->id);
    entry.our_id      = git_oid_tostr_s(&our->id);

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
    bool status = true;

    assert(entries.size() == paths.size());

    for (std::size_t i = 0; i < entries.size(); ++i) {
        status &= apply_resolution(paths[i], entries[i], repo, index);
    }

    if (git_index_write(index) != 0) {
        return false;
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

    // The file is deleted
    if (resolution_id.empty()) {
        return git_index_remove_bypath(index, path.c_str()) == 0;
    }

    git_oid oid;
    git::blob_t blob;

    if (git_oid_fromstr(&oid, resolution_id.c_str()) != 0) {
        return false;
    }

    if (git_blob_lookup(&blob, repo, &oid) != 0) {
        return false;
    }

    {
        const auto* content            = reinterpret_cast<const char*>(git_blob_rawcontent(blob.get()));
        const std::size_t content_size = git_blob_rawsize(blob.get());

        std::ofstream file(path, std::ios::out | std::ios::binary);
        if (!file.good()) {
            git_error_set_str(GIT_ERROR_FILESYSTEM, "Could not open file");
            return false;
        }

        file.write(content, static_cast<std::streamsize>(content_size));
        if (!file.good()) {
            git_error_set_str(GIT_ERROR_FILESYSTEM, "Could not write to file");
            return false;
        }
    }

    return git_index_add_bypath(index, path.c_str()) == 0;
}

void ConflictManager::add_resolution(const ConflictEntry& entry, std::string id) { m_conflicts[entry] = id; }

void ConflictManager::add_commits_resolution(const ConflictCommits& conflict, core::git::tree_t&& resolution) {
    m_commits[conflict] = std::move(resolution);
}

git_tree* ConflictManager::get_commits_resolution(const git_commit* old_commit, const git_commit* new_commit) {
    ConflictCommits commits;

    commits.parent_id = git::format_oid_to_str<git::OID_SIZE>(git_commit_id(old_commit));
    commits.child_id  = git::format_oid_to_str<git::OID_SIZE>(git_commit_id(new_commit));

    return get_commits_resolution(commits);
}

git_tree* ConflictManager::get_commits_resolution(const ConflictCommits& conflict) {
    auto it = m_commits.find(conflict);
    if (it != m_commits.end()) {
        return it->second.get();
    }

    return nullptr;
}

}
