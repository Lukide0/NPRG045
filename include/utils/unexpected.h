#pragma once

#include <cstdlib>
#include <format>
#include <iostream>
#include <source_location>
#include <string_view>

/**
 * @brief Marks unreachable or invalid code paths.
 *
 * @param msg Optional diagnostic message.
 * @param loc Source location of the call.
 *
 * @note Terminates the program.
 */
[[noreturn]] inline void UNEXPECTED(std::string_view msg = "", std::source_location loc = std::source_location()) {
    std::cerr << std::format("UNEXPECTED[{}:{}]: {}", loc.file_name(), loc.line(), msg) << std::endl;
    std::abort();
}
