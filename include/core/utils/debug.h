#pragma once

#include "core/git/error.h"
#include "logging/Log.h"
#include <git2/errors.h>
#include <source_location>

namespace core::utils {

inline void log_libgit_error(std::source_location loc = std::source_location::current()) {
    logging::Log::error(git::get_last_error(), loc);
}

}
