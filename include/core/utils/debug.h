#pragma once

#include <format>
#include <git2/errors.h>
#include <iostream>
#include <ostream>
#include <source_location>

namespace core::utils {

inline void print_last_error(std::source_location loc = std::source_location::current()) {
    const auto* err = git_error_last();

    std::cerr << std::format("ERROR[{}:{}]: {}", loc.file_name(), loc.line(), err->message) << std::endl;
}

}
