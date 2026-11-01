#pragma once

#include "core/action/Action.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebaseSession.h"
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
 * @param rebase Rebase sesion.
 * @param out_first Resulting first commit.
 * @param out_second Resulting second commit.
 * @param step The input step.
 * @param previous_step The previous step.
 * @param patch Diff patch used for splitting.
 *
 * @return Error status.
 */
SplitError split(
    rebase::RebaseSession& rebase,
    git::commit_t& out_first,
    git::commit_t& out_second,
    const rebase::StepResult& step,
    const rebase::StepResult* previous_step,
    git::diff_t& patch
);

}
