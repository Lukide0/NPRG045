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
    static bool is_debug();

    /**
     * @brief Enables or disables verbose logging to terminal.
     *
     * @param enable Whether verbose logging is enabled.
     */
    static void enable_verbose(bool verbose);
    static bool is_verbose();

    /**
     * @brief Sets the active log filter. The filter is modified by enable_verbose().
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

#define LOG_DISPATCH_IMPL(FN, FMT) FN(FMT)
#define LOG_DISPATCH_IMPL_FMT(FN, FMT, ...) FN(std::format(FMT, __VA_ARGS__))

#define LOG_DISPATCH(FN, FMT, ...) LOG_DISPATCH_IMPL##__VA_OPT__(_FMT)(FN, FMT __VA_OPT__(, ) __VA_ARGS__)

/**
 * @brief Logs an error message (formatted).
 */
#define LOG_ERROR(...) LOG_DISPATCH(::logging::Log::error, __VA_ARGS__)

/**
 * @brief Logs an info message (formatted).
 */
#define LOG_INFO(...) LOG_DISPATCH(::logging::Log::info, __VA_ARGS__)

/**
 * @brief Logs a warning message (formatted).
 */
#define LOG_WARN(...) LOG_DISPATCH(::logging::Log::warn, __VA_ARGS__)

}
