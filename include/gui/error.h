#pragma once

#include "logging/Log.h"
#include <format>
#include <string>
#include <utility>

#include <QMessageBox>
#include <QObject>

namespace gui {

class ErrorMessage {
public:
    [[nodiscard]] const std::string& message() const { return m_msg; }

    [[nodiscard]] const std::string& title() const { return m_title; }

    static void
    error(QWidget* parent, const ErrorMessage& msg, std::source_location loc = std::source_location::current()) {
        error(parent, msg.title(), msg.message(), loc);
    }

    static void error(
        QWidget* parent,
        const char* title,
        const char* message,
        std::source_location loc = std::source_location::current()
    ) {
        error(parent, QString::fromUtf8(title), QString::fromUtf8(message), loc);
    }

    static void error(
        QWidget* parent,
        const std::string& title,
        const std::string& message,
        std::source_location loc = std::source_location::current()
    ) {
        ::logging::Log::error(std::format("{}: {}", title, message), loc);

        QMessageBox::critical(
            parent, QString::fromStdString(title), QString::fromStdString(message), QMessageBox::Ok, QMessageBox::Ok
        );
    }

    static void error(
        QWidget* parent,
        const QString& title,
        const QString& message,
        std::source_location loc = std::source_location::current()
    ) {
        QString single_line = message;
        single_line.replace("\r\n", "\\n");
        single_line.replace("\r", "\\r");
        single_line.replace("\n", "\\n");

        ::logging::Log::error((QStringLiteral("%1: %2").arg(title).arg(single_line)).toStdString(), loc);

        QMessageBox::critical(parent, title, message, QMessageBox::Ok, QMessageBox::Ok);
    }

    static void warn(
        QWidget* parent,
        const std::string& title,
        const std::string& message,
        std::source_location loc = std::source_location::current()
    ) {
        ::logging::Log::warn(std::format("{}: {}", title, message), loc);

        QMessageBox::warning(parent, QString::fromStdString(title), QString::fromStdString(message));
    }

    static void warn(
        QWidget* parent,
        const char* title,
        const char* message,
        std::source_location loc = std::source_location::current()
    ) {
        warn(parent, QString::fromUtf8(title), QString::fromUtf8(message), loc);
    }

    static void warn(
        QWidget* parent,
        const QString& title,
        const QString& message,
        std::source_location loc = std::source_location::current()
    ) {
        QString single_line = message;
        single_line.replace("\r\n", "\\n");
        single_line.replace("\r", "\\r");
        single_line.replace("\n", "\\n");

        ::logging::Log::warn((QStringLiteral("%1: %2").arg(title).arg(single_line)).toStdString(), loc);

        QMessageBox::warning(parent, title, message);
    }

private:
    std::string m_title;
    std::string m_msg;

    ErrorMessage(std::string title, std::string msg)
        : m_title(std::move(title))
        , m_msg(std::move(msg)) { }
};

#define DISPLAY_ERROR(PARENT, TITLE, MESSAGE) ::gui::ErrorMessage::error((PARENT), (TITLE), (MESSAGE))
#define DISPLAY_WARN(PARENT, TITLE, MESSAGE) ::gui::ErrorMessage::warn((PARENT), (TITLE), (MESSAGE))

}
