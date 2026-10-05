#include "logging/Log.h"

#include "build.h"

#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QtLogging>
#include <QtTypes>
#include <QUuid>

namespace logging {

static auto g_filter         = static_cast<int>(Type::INFO | Type::WARN | Type::ERR);
static bool g_enable_debug   = false;
static bool g_enable_verbose = false;
static std::unique_ptr<QLockFile> g_log_lock;

static QFile g_log_file;
static QTextStream g_log_stream;

static void handle_qt_message(QtMsgType type, const QMessageLogContext& context, const QString& msg);
static void write_to_log(
    const char* prefix, const char* file_name, const char* function_name, std::uint32_t line, const std::string& msg
);

static void cleanup_logs(const QDir& dir);

void Log::enable_debug(bool enable) { g_enable_debug = enable; }

bool Log::is_debug() { return g_enable_debug; }

bool Log::is_verbose() { return g_enable_verbose; }

void Log::enable_verbose(bool verbose) {
    g_enable_verbose = verbose;
    if (verbose) {
        g_filter = static_cast<int>(Type::INFO | Type::WARN | Type::ERR);
    } else {
        g_filter = static_cast<int>(Type::ERR);
    }
}

void Log::set_filter(Type type) { g_filter = static_cast<int>(type); }

void Log::init() {
    const QDir directory = QStandardPaths::writableLocation(QStandardPaths::StandardLocation::StateLocation);

    if (!directory.exists()) {
        // creates directories
        if (!directory.mkpath(".")) {
            std::cerr << "ERROR: Failed to create log directory in " << directory.path().toStdString() << '\n';
            return;
        }
    }

    cleanup_logs(directory);

    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    const qint64 pid        = QApplication::applicationPid();

    const QString log_path = directory.filePath(QString("events-%1-p%2.log").arg(timestamp).arg(pid));

    g_log_lock = std::make_unique<QLockFile>(log_path + ".lock");
    g_log_lock->setStaleLockTime(0);

    if (!g_log_lock->tryLock()) {
        std::cerr << "ERROR: Failed to acquire log lock\n";
        g_log_lock.reset();
        return;
    }

    g_log_file.setFileName(log_path);
    if (!g_log_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << "ERROR: Failed to open log file at " << log_path.toStdString() << '\n';

        g_log_lock->unlock();
        g_log_lock.reset();
        return;
    }

    g_log_stream.setDevice(&g_log_file);
    qInstallMessageHandler(handle_qt_message);
}

void cleanup_logs(const QDir& dir) {
    // keep only 20 log files
    constexpr std::uint32_t MAX_OLD_LOGS = 20;

    // filter logs
    const auto files = dir.entryInfoList({ "events-*.log" }, QDir::Files, QDir::Time | QDir::Reversed);

    std::uint32_t kept = 0;
    for (const auto& file : files) {
        const auto log_path  = file.absoluteFilePath();
        const auto lock_path = log_path + ".lock";

        QLockFile lock(lock_path);
        lock.setStaleLockTime(0);

        if (!lock.tryLock()) {
            // active log
            continue;
        }

        if (kept < MAX_OLD_LOGS) {
            kept += 1;
        } else {
            QFile::remove(log_path);
        }

        lock.unlock();
    }
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
