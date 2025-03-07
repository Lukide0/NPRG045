#pragma once

#include <cassert>
#include <cstdlib>
#include <format>
#include <iostream>
#include <source_location>
#include <string_view>

[[noreturn]] inline void TODO(std::string_view msg = "", std::source_location loc = std::source_location()) {
    std::cerr << std::format("TODO[{}:{}]: {}", loc.file_name(), loc.line(), msg) << std::endl;
    std::abort();
}
