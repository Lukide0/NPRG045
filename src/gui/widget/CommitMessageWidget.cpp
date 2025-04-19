#include "gui/widget/CommitMessageWidget.h"
#include "action/ActionManager.h"

CommitMessageWidget::CommitMessageWidget(ActionsManager& manager, QWidget* parent)
    : QWidget(parent)
    , m_manager(manager) {
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
    });
}

void CommitMessageWidget::setAction(Action* action) {
    QString msg;
    m_action = action;

    auto msg_id = action->get_msg_id();
    if (action->has_msg() && msg_id.is_value()) {
        msg = QString::fromStdString(m_manager.get_msg(msg_id.value()));
    } else {
        msg = git_commit_message(action->get_commit());
    }

    m_editor->setReadOnly(!action->can_edit_msg());

    m_editor->blockSignals(true);
    m_editor->setPlainText(msg);
    m_editor->blockSignals(false);
}
