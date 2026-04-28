#pragma once

#include <functional>
#include <git2/index.h>
#include <git2/types.h>

namespace core::conflict {

/**
 * @brief Represents a triple of index entries.
 */
struct entry_data_t {
    const git_index_entry* ancestor;
    const git_index_entry* their;
    const git_index_entry* our;
};

/**
 * @brief Iterates over conflict entries in a Git index.
 *
 * @param index Git index to iterate.
 * @param entry Callback invoked for each coflict entry.
 *
 * @return True if iteration completed successfully.
 */
bool iterate(git_index* index, std::function<bool(entry_data_t)> entry);

/**
 * @brief Iterates over all index entries.
 *
 * @param index Git index to iterate.
 * @param entry Callback invoked for each index entry.
 *
 * @return True if iteration completed successfully.
 */
bool iterate_all(git_index* index, std::function<bool(const git_index_entry*)> entry);

}
