#pragma once

#include <functional>
#include <git2/index.h>
#include <git2/types.h>

namespace core::conflict {

struct entry_data_t {
    const git_index_entry* ancestor;
    const git_index_entry* their;
    const git_index_entry* our;
};

bool iterate(git_index* index, std::function<bool(entry_data_t)> entry);

bool iterate_all(git_index* index, std::function<bool(const git_index_entry*)> entry);

}
