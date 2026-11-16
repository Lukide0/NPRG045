#pragma once

#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace core {

struct Error {
    std::string title;
    std::string message;

    bool operator==(const Error&) const = default;
};

struct Warns {
    Warns() = default;

    Warns(const std::vector<std::string>& w)
        : messages(w) { }

    Warns(std::vector<std::string>&& w)
        : messages(std::move(w)) { }

    std::vector<std::string> messages;
};

template <typename T> using Result = std::expected<T, Error>;

template <typename T> struct WResult {
    Result<T> result;
    Warns warns;
};

}
