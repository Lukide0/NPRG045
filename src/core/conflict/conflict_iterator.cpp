#include "core/conflict/conflict_iterator.h"
#include "core/git/types.h"
#include "core/utils/debug.h"
#include <functional>
#include <git2/index.h>
#include <git2/types.h>
#include <utility>

namespace core::conflict {

bool iterate(git_index* index, std::function<bool(entry_data_t)> entry) {

    git::conflict_iterator_t iter;

    if (git_index_conflict_iterator_new(&iter, index) != 0) {
        return false;
    }

    const git_index_entry* ancestor;
    const git_index_entry* their;
    const git_index_entry* our;

    int status = git_index_conflict_next(&ancestor, &our, &their, iter.get());
    for (; status == 0; status = git_index_conflict_next(&ancestor, &our, &their, iter.get())) {
        bool res = entry(
            entry_data_t {
                .ancestor = ancestor,
                .their    = their,
                .our      = our,
            }
        );

        if (!res) {
            break;
        }
    }

    return true;
}

bool iterate_all(git_index* index, std::function<bool(const git_index_entry*)> entry) {
    git::index_iterator_t iter;

    if (git_index_iterator_new(&iter, index) != 0) {
        return false;
    }

    const git_index_entry* item;

    int status = git_index_iterator_next(&item, iter.get());
    for (; status == 0; status = git_index_iterator_next(&item, iter.get())) {
        bool res = entry(item);

        if (!res) {
            break;
        }
    }

    return true;
}

}
