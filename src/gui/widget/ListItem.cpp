#include "gui/widget/ListItem.h"
#include "core/git/parser.h"
#include "core/state/CommandHistory.h"
#include "gui/widget/RebaseViewWidget.h"

#include <QBoxLayout>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QListWidgetItem>
#include <QPalette>
#include <QWidget>

#include <cassert>
#include <memory>

ListItem::ListItem(RebaseViewWidget* rebase, QListWidget* list, int row)
    : m_rebase(rebase)
    , m_parent(list)
    , m_row(row) {

    m_combo = new QComboBox();

    for (auto&& item : items) {
        m_combo->addItem(cmd_to_str(item), static_cast<int>(item));
    }

    m_text = new QLabel();
    m_text->setMargin(0);
    m_text->setContentsMargins(0, 0, 0, 0);

    m_layout = new QHBoxLayout();
    m_layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    m_layout->setContentsMargins(2, 2, 2, 2);
    m_layout->addWidget(m_combo);
    m_layout->addWidget(m_text, 1);

    setLayout(m_layout);

    connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int index) {
        assert(index != -1);

        auto raw_type  = m_combo->itemData(index).toInt();
        auto curr_type = static_cast<CmdType>(raw_type);
        auto prev_type = m_action.type;

        if (curr_type == prev_type) {
            return;
        }

        m_action.type = curr_type;

        core::state::CommandHistory::Add(
            std::make_unique<ListItemChangedCommand>(m_rebase, m_parent, m_row, prev_type, curr_type)
        );
    });
}

void ListItem::setCommitAction(const CommitAction& action) {
    m_action = action;

    auto index = indexOf(action.type);
    assert(index != -1);

    m_combo->setCurrentIndex(index);
}

ListItemMoveCommand::ListItemMoveCommand(RebaseViewWidget* rebase, QListWidget* parent, int prev_row, int curr_row)
    : m_rebase(rebase)
    , m_parent(parent)
    , m_prev(prev_row)
    , m_curr(curr_row) { }

void ListItemMoveCommand::execute() {
    move(m_prev, m_curr);
    m_rebase->updateActions();
}

void ListItemMoveCommand::undo() {
    move(m_curr, m_prev);
    m_rebase->updateActions();
}

void ListItemMoveCommand::move(int from, int to) {
    auto* tmp_item = m_parent->item(from);
    auto* widget   = m_parent->itemWidget(tmp_item);

    auto* item = m_parent->takeItem(from);

    assert(widget != nullptr);

    m_parent->insertItem(to, item);
    m_parent->setItemWidget(item, widget);
}

ListItemChangedCommand::ListItemChangedCommand(
    RebaseViewWidget* rebase, QListWidget* parent, int row, CmdType prev, CmdType curr
)
    : m_rebase(rebase)
    , m_parent(parent)
    , m_row(row)
    , m_prev(prev)
    , m_curr(curr) { }

void ListItemChangedCommand::execute() {
    set_type(m_curr);
    m_rebase->updateActions();
}

void ListItemChangedCommand::undo() {
    set_type(m_prev);
    m_rebase->updateActions();
}

void ListItemChangedCommand::set_type(CmdType type) {
    auto* item      = m_parent->item(m_row);
    auto* list_item = dynamic_cast<ListItem*>(m_parent->itemWidget(item));

    list_item->setActionTypeNoSignal(type);
}
