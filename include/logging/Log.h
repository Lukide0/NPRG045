#pragma once

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
    /**
     * @brief Initializes the logging system.
     */
    static void init();

    /**
     * @brief Enables or disables debug logging to terminal.
     *
     * @param enable Whether debug logging is enabled.
     */
    static void enable_debug(bool enable);

    /**
     * @brief Sets the active log filter.
     *
     * @param type Log types to allow.
     *
     * @details The filter is not applied to the log file.
     */
    static void set_filter(Type type);

    /**
     * @brief Logs an error message.
     *
     * @param msg Log message.
     * @param location Source location of the log call.
     */
    static void error(const std::string& msg, std::source_location location = std::source_location::current()) {
        message(std::cerr, Type::ERR, msg, location);
    }

    /**
     * @brief Logs an info message.
     *
     * @param msg Log message.
     * @param location Source location of the log call.
     */
    static void info(const std::string& msg, std::source_location location = std::source_location::current()) {
        message(std::cout, Type::INFO, msg, location);
    }

    /**
     * @brief Logs a warning message.
     *
     * @param msg Log message.
     * @param location Source location of the log call.
     */
    static void warn(const std::string& msg, std::source_location location = std::source_location::current()) {
        message(std::cerr, Type::WARN, msg, location);
    }

private:
    static void message(std::ostream& stream, Type type, const std::string& msg, std::source_location location);
};

/**
 * @brief Logs an error message (formatted).
 */
#define LOG_ERROR(...) ::logging::Log::error(std::format(__VA_ARGS__))

/**
 * @brief Logs an info message (formatted).
 */
#define LOG_INFO(...) ::logging::Log::info(std::format(__VA_ARGS__))

/**
 * @brief Logs a warning message (formatted).
 */
#define LOG_WARN(...) ::logging::Log::warn(std::format(__VA_ARGS__))

}
