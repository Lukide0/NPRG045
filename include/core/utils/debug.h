#pragma once

#include "logging/Log.h"
#include <git2/errors.h>
#include <source_location>

namespace core::utils {

inline void log_libgit_error(std::source_location loc = std::source_location::current()) {
    const auto* err = git_error_last();

    if (err == nullptr) {
        return;
    }

    const char* msg = "Unknown error";
    if (err->message != nullptr) {
        msg = err->message;
    }

    logging::Log::error(msg, loc);
}

}
