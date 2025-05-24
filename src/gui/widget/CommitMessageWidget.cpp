#include "gui/widget/CommitMessageWidget.h"
#include "action/Action.h"
#include "action/ActionManager.h"
#include "core/utils/optional_uint.h"

#include <cstddef>
#include <format>
#include <git2/commit.h>
#include <string>

#include <QAction>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QString>

namespace gui::widget {

using action::Action;
using action::ActionType;

CommitMessageWidget::CommitMessageWidget(QWidget* parent)
    : QWidget(parent)
    , m_manager(action::ActionsManager::get()) {
    m_layout = new QHBoxLayout();
    setLayout(m_layout);

    m_editor = new QPlainTextEdit();
    m_editor->setReadOnly(true);

    m_layout->addWidget(m_editor);

    connect(m_editor, &QPlainTextEdit::textChanged, this, [this]() {
        if (m_action == nullptr || !m_action->can_edit_msg()) {
            return;
        }

        auto text = m_editor->toPlainText().toStdString();

        auto id = m_action->get_msg_id();
        if (id.is_none()) {
            id = optional_u31::some(m_manager.add_msg(text));
        } else {
            auto& str = m_manager.get_msg(id.value());
            str       = text;
        }

        m_action->set_msg_id(id);

        if (m_handle != nullptr) {
            m_handle(text);
        }
    });
}

QString create_commit_message(Action* action) {
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

    Action* curr      = action->get_next();
    std::size_t count = 1;
    bool exit         = false;

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
            msg += std::format("# This is the commit message #{}:\n", count);
            msg += '\n';
            msg += git_commit_message(curr->get_commit());
            msg += '\n';
            break;
        }

        curr = curr->get_next();
    }

    if (count > 1) {
        msg = std::format("# This is a combination of {} commits.\n# This is the commit message #1:\n{}", count, msg);
    }

    return QString::fromStdString(msg);
}

void CommitMessageWidget::setAction(Action* action) {
    QString msg;
    m_action = action;

    auto msg_id = action->get_msg_id();
    if (action->has_msg() && msg_id.is_value()) {
        msg = QString::fromStdString(m_manager.get_msg(msg_id.value()));
    } else {
        msg = create_commit_message(action);
    }

    m_editor->setReadOnly(!action->can_edit_msg());

    m_editor->blockSignals(true);
    m_editor->setPlainText(msg);
    m_editor->blockSignals(false);
}

}
