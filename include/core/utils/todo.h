#pragma once

#include <cassert>
#include <cstdlib>
#include <format>
#include <iostream>
#include <source_location>
#include <string_view>

/**
 * @brief Marks unreachable or unfinished code path.
 *
 * @param msg Optional message.
 * @param loc Source location of the call.
 *
 * @note Terminates the program.
 */
[[noreturn]] inline void TODO(std::string_view msg = "", std::source_location loc = std::source_location()) {
    std::cerr << std::format("TODO[{}:{}]: {}", loc.file_name(), loc.line(), msg) << std::endl;
    std::abort();
}
