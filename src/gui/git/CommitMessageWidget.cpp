#include "gui/git/CommitMessageWidget.h"

#include "core/action/Action.h"
#include "core/git/types.h"
#include "gui/error.h"
#include "gui/file.h"

#include <cstddef>
#include <format>
#include <string>

#include <git2/commit.h>

#include <QAction>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <string_view>

namespace gui::git {

using core::action::Action;
using core::action::ActionType;

CommitMessageWidget::CommitMessageWidget(QWidget* parent)
    : QWidget(parent) {
    m_layout = new QHBoxLayout();
    setLayout(m_layout);

    m_message_timer.setSingleShot(true);
    m_message_timer.setInterval(300);

    m_editor = new QPlainTextEdit();
    m_editor->setReadOnly(true);

    auto* buttons = new QVBoxLayout();
    m_open_editor = new QPushButton("Open in editor");
    m_update_msg  = new QPushButton("Update from file");

    m_open_editor->setEnabled(false);
    m_update_msg->setEnabled(false);

    buttons->addWidget(m_open_editor);
    buttons->addWidget(m_update_msg);
    buttons->addStretch();

    m_layout->addWidget(m_editor);
    m_layout->addLayout(buttons);

    connect(m_editor, &QPlainTextEdit::textChanged, this, [this]() { m_message_timer.start(); });
    connect(&m_message_timer, &QTimer::timeout, this, [this]() {
        auto text = m_editor->toPlainText();
        emit commitMessageChanged(text);
    });

    connect(m_open_editor, &QPushButton::clicked, this, [this]() { this->openInEditor(); });
    connect(m_update_msg, &QPushButton::clicked, this, [this]() { this->updateText(); });
}

QString create_commit_message(const Action* action, std::string_view comment_str) {
    std::string msg = git_commit_message(action->get_commit());

    switch (action->get_type()) {
    case ActionType::PICK:
    case ActionType::EDIT:
    case ActionType::REWORD:
        break;
    case ActionType::DROP:
    case ActionType::SQUASH:
    case ActionType::FIXUP:
        return QString::fromStdString(msg);
    }

    const Action* curr = action->get_next();
    std::size_t count  = 1;
    bool exit          = false;

    while (curr != nullptr && !exit) {

        switch (curr->get_type()) {
        case ActionType::PICK:
        case ActionType::FIXUP:
        case ActionType::REWORD:
        case ActionType::EDIT:
            exit = true;
            break;
        case ActionType::DROP:
            break;
        case ActionType::SQUASH:
            count += 1;
            msg += std::format("{} This is the commit message #{}:\n", comment_str, count);
            msg += '\n';
            msg += git_commit_message(curr->get_commit());
            msg += '\n';
            break;
        }

        curr = curr->get_next();
    }

    if (count > 1) {
        msg = std::format(
            "{} This is a combination of {} commits.\n# This is the commit message #1:\n{}", comment_str, count, msg
        );
    }

    return QString::fromStdString(msg);
}

void CommitMessageWidget::setAction(const Action* action, std::string_view comment_str) {
    QString msg;

    if (action->can_edit_msg() && action->has_msg()) {
        msg = QString::fromStdString(*action->get_msg());
    } else {
        msg = create_commit_message(action, comment_str);
    }

    m_oid = QString::fromStdString(core::git::format_oid_to_str<core::git::OID_SIZE>(&action->get_oid()));
    m_editor->setReadOnly(!action->can_edit_msg());

    m_editor->blockSignals(true);
    m_editor->setPlainText(msg);
    m_editor->blockSignals(false);
}

void CommitMessageWidget::openInEditor() {
    // already opened
    if (!m_filepath.isEmpty()) {
        bool status = open_temp_file(m_filepath);
        if (!status) {
            QMessageBox::warning(
                this,
                "Editor Error",
                QStringLiteral(
                    "Failed to open the temporary file in editor.\n"
                    "The file may have been deleted or moved.\n"
                    "File: %1"
                )
                    .arg(m_filepath),
                QMessageBox::Ok
            );

            m_filepath.clear();
        }

        return;
    }

    auto&& [filepath, status] = create_temp_file(m_editor->toPlainText(), m_oid, true);
    if (!status) {
        DISPLAY_ERROR(this, "Editor error", "Failed to create temporary file for editing.");
        return;
    }

    m_filepath = filepath;
}

void CommitMessageWidget::updateText() {
    if (m_oid.isEmpty()) {
        return;
    }

    // editor is not open
    if (m_filepath.isEmpty()) {
        return;
    }

    auto&& [content, status] = read_file(m_filepath);

    if (!status) {
        DISPLAY_ERROR(
            this,
            "File read error",
            std::format(
                "Failed to read the edited file.\nThe file may have been deleted or is no longer accessible.\nFile: {}",
                m_filepath.toStdString()
            )
        );

        m_filepath.clear();
        return;
    }

    setEditorText(content);

    emit commitMessageChanged(content);
}

}
