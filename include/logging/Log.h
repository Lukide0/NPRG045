#pragma once

#include <format>
#include <iostream>
#include <source_location>
#include <string>

namespace logging {

enum class Type {
    NONE = 0,
    ERR  = 1,
    INFO = 2,
    WARN = 4,
};

constexpr Type operator|(Type a, Type b) { return static_cast<Type>(static_cast<int>(a) | static_cast<int>(b)); }

class Log {
public:
    static void enable_debug(bool enable);
    static void set_filter(Type type);

    static void error(const std::string& msg, std::source_location location = std::source_location::current()) {
        message(std::cerr, Type::ERR, msg, location);
    }

    static void info(const std::string& msg, std::source_location location = std::source_location::current()) {
        message(std::cout, Type::INFO, msg, location);
    }

    static void warn(const std::string& msg, std::source_location location = std::source_location::current()) {
        message(std::cerr, Type::ERR, msg, location);
    }

private:
    static void message(std::ostream& stream, Type type, const std::string& msg, std::source_location location);
};

#define LOG_ERROR(...) ::logging::Log::error(std::format(__VA_ARGS__))
#define LOG_INFO(...) ::logging::Log::info(std::format(__VA_ARGS__))
#define LOG_WARN(...) ::logging::Log::warn(std::format(__VA_ARGS__))

}
