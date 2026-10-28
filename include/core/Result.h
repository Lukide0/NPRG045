#pragma once

#include <expected>
#include <string>

namespace core {

struct Error {
    std::string title;
    std::string message;
};

template <typename T> using Result = std::expected<T, Error>;

}
