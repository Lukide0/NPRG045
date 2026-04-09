#include "logging/Log.h"

#include "build.h"

#include <format>
#include <iostream>

#include <QDir>
#include <QFile>
#include <QThread>
#include <QtLogging>

namespace logging {

static auto g_filter       = static_cast<int>(Type::INFO | Type::WARN | Type::ERR);
static bool g_enable_debug = false;

static QFile g_log_file;
static QTextStream g_log_stream;

static void handle_qt_message(QtMsgType type, const QMessageLogContext& context, const QString& msg);
static void write_to_log(
    const char* prefix, const char* file_name, const char* function_name, std::uint32_t line, const std::string& msg
);

void Log::enable_debug(bool enable) { g_enable_debug = enable; }

void Log::set_filter(Type type) { g_filter = static_cast<int>(type); }

void Log::init() {
    const QDir directory = QDir::homePath() + "/.local/share/git_shuffle";
    const QString path   = directory.path() + "/events.log";

    if (!directory.exists()) {
        // creates directories
        if (!directory.mkpath(".")) {
            std::cerr << "ERROR: Failed to create log directory in " << directory.path().toStdString() << '\n';
            return;
        }
    }

    g_log_file.setFileName(path);
    if (!g_log_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        std::cerr << "ERROR: Failed to open log file at " << path.toStdString() << '\n';
        return;
    }

    g_log_stream.setDevice(&g_log_file);
    qInstallMessageHandler(handle_qt_message);
}

void handle_qt_message(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    const char* prefix = "";
    switch (type) {
    // ignore debug messages
    case QtDebugMsg:
        return;
    case QtWarningMsg:
        prefix = "WARN";
        break;
    case QtCriticalMsg:
        prefix = "ERROR";
        break;
    case QtFatalMsg:
        prefix = "FATAL";
        break;
    case QtInfoMsg:
        prefix = "INFO";
        break;
    }

    write_to_log(prefix, context.file, context.function, context.line, msg.toStdString());
}

void write_to_log(
    const char* prefix, const char* file_name, const char* function_name, std::uint32_t line, const std::string& msg
) {
    if (g_log_stream.device() == nullptr) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    const QString thread_id = QString("0x%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);

    g_log_stream << '[' << timestamp << "][" << thread_id << "][" << prefix << ']';

    if (file_name != nullptr) {
        std::string_view path = build::remove_source_dir(file_name);

        const char* function = (function_name != nullptr) ? function_name : "<unknown>";

        g_log_stream << '[' << path.data() << ':' << line << "](" << function << ')';
    }

    g_log_stream << ": " << QString::fromStdString(msg) << '\n';
    g_log_stream.flush();
}

void Log::message(std::ostream& stream, Type type, const std::string& msg, std::source_location location) {

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

    write_to_log(prefix, location.file_name(), location.function_name(), location.line(), msg);

    bool disabled = (static_cast<int>(type) & g_filter) == 0;
    if (disabled) {
        return;
    }

    if (g_enable_debug) {
        stream << std::format(
            "{}[{}:{}]({}): {}\n",
            prefix,
            build::remove_source_dir(location.file_name()),
            location.line(),
            location.function_name(),
            msg
        );
    } else {
        stream << std::format("{}: {}\n", prefix, msg);
    }
}
}
