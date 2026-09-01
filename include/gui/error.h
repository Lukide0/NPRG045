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
    static ErrorMessage create_libgit(std::string title, std::string msg) {
        return { std::move(title), std::move(msg), false };
    }

    static ErrorMessage create(std::string title, std::string msg) {
        return { std::move(title), std::move(msg), true };
    }

    [[nodiscard]] bool is_libgit() const { return !m_internal; }

    [[nodiscard]] const std::string& message() const { return m_msg; }

    [[nodiscard]] const std::string& title() const { return m_title; }

    static void
    display(QWidget* parent, const ErrorMessage& msg, std::source_location loc = std::source_location::current()) {
        if (msg.is_libgit()) {
            display<true>(parent, msg.title(), msg.message(), loc);
        } else {
            display<false>(parent, msg.title(), msg.message(), loc);
        }
    }

    template <bool libgit>
    static void display(
        QWidget* parent,
        const std::string& title,
        const std::string& message,
        std::source_location loc = std::source_location::current()
    ) {
        if constexpr (libgit) {
            ::logging::Log::error(std::format("[Libgit2]{}: {}", title, message), loc);
        } else {
            ::logging::Log::error(std::format("{}: {}", title, message), loc);
        }

        QMessageBox::critical(
            parent, QString::fromStdString(title), QString::fromStdString(message), QMessageBox::Ok, QMessageBox::Ok
        );
    }

private:
    bool m_internal;
    std::string m_title;
    std::string m_msg;

    ErrorMessage(std::string title, std::string msg, bool internal)
        : m_internal(internal)
        , m_title(std::move(title))
        , m_msg(std::move(msg)) { }
};

#define DISPLAY_ERROR(PARENT, TITLE, MESSAGE) ::gui::ErrorMessage::display<false>((PARENT), (TITLE), (MESSAGE))
#define DISPLAY_LIBGIT_ERROR(PARENT, TITLE, MESSAGE) ::gui::ErrorMessage::display<true>((PARENT), (TITLE), (MESSAGE))

}
