#include "logging/Log.h"
#include <format>

namespace logging {

static auto g_filter       = static_cast<int>(Type::INFO | Type::WARN | Type::ERR);
static bool g_enable_debug = false;

void Log::enable_debug(bool enable) { g_enable_debug = enable; }

void Log::set_filter(Type type) { g_filter = static_cast<int>(type); }

void Log::message(std::ostream& stream, Type type, const std::string& msg, std::source_location location) {

    bool disabled = (static_cast<int>(type) & g_filter) == 0;

    if (disabled) {
        return;
    }

    const char* prefix;
    switch (type) {
    case Type::ERR:
        prefix = "ERROR";
        break;
    case Type::INFO:
        prefix = "INFO";
        break;
    case Type::WARN:
        prefix = "WARN";
        break;
    case Type::NONE:
        prefix = "";
        break;
    }

    if (g_enable_debug) {
        stream << std::format(
            "{}[{}:{}]({}): {}\n", prefix, location.file_name(), location.line(), location.function_name(), msg
        );
    } else {
        stream << std::format("{}: {}\n", prefix, msg);
    }
}
}
