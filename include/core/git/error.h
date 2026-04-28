#pragma once

#include <git2/errors.h>
#include <string>

namespace core::git {

/**
 * @brief Retrieves the last Git error message.
 *
 * @return Error message string or "Unknown error" if none is available.
 */
inline std::string get_last_error() {
    const auto* err = git_error_last();
    if (err == nullptr || err->message == nullptr) {
        return "Unknown error";
    }

    return err->message;
}

}
