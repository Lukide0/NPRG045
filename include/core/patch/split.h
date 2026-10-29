#pragma once

#include "core/action/Action.h"
#include "core/git/types.h"
#include <string>
#include <string_view>
#include <utility>

namespace core::patch {

struct SplitError {
    SplitError() = default;

    SplitError(std::string_view t, std::string msg)
        : title(t)
        , message(std::move(msg)) { }

    std::string_view title;
    std::string message;

    [[nodiscard]] bool has_error() const { return !title.empty(); }
};

/**
 * @brief Splits a commit into two parts using a patch.
 *
 * @param out_first Resulting first commit.
 * @param out_second Resulting second commit.
 * @param actions Actions manager.
 * @param act Action associated with the split.
 * @param patch Diff patch used for splitting.
 *
 * @return Error status.
 */
SplitError split(
    git::commit_t& out_first,
    git::commit_t& out_second,
    action::ActionsManager& actions,
    action::Action* act,
    git::diff_t& patch
);

}
