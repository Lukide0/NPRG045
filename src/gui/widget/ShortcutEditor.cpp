#include "gui/widget/ShortcutEditor.h"
#include "App.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::widget {

ShortcutEditor::ShortcutEditor(QWidget* parent)
    : QWidget(parent)
    , m_actions(App::getShortcuts()) {
    setup();
}

void ShortcutEditor::setup() {
    auto* layout = new QVBoxLayout(this);

    auto* info_label = new QLabel("Double-click a shortcut to edit it.");
    info_label->setWordWrap(true);
    layout->addWidget(info_label);

    m_table = new QTableWidget(this);
    {
        m_table->setColumnCount(3);
        m_table->setHorizontalHeaderLabels(
            {
                "Action",
                "Description",
                "Shortcut",
            }
        );

        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        // hide row numbers
        m_table->verticalHeader()->setVisible(false);

        auto* header = m_table->horizontalHeader();

        header->setStretchLastSection(false);
        header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(1, QHeaderView::Stretch);
        header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    }

    connect(m_table, &QTableWidget::cellDoubleClicked, this, &ShortcutEditor::onEditShortcut);

    layout->addWidget(m_table);

    m_btn_reset = new QPushButton("Reset to Defaults", this);
    connect(m_btn_reset, &QPushButton::clicked, this, &ShortcutEditor::onResetToDefaults);

    layout->addWidget(m_btn_reset);

    populateTable();
}

void ShortcutEditor::populateTable() {
    m_table->setRowCount(m_actions.size());

    int row = 0;
    for (auto it = m_actions.constBegin(); it != m_actions.constEnd(); ++it, ++row) {

        // action ID
        auto* item_id = new QTableWidgetItem(it.key());
        item_id->setFlags(item_id->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 0, item_id);

        // description
        auto* item_desc = new QTableWidgetItem(it.value().description);
        item_desc->setFlags(item_desc->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 1, item_desc);

        // shortcut
        auto* item_shortcut = new QTableWidgetItem(it.value().action->shortcut().toString(QKeySequence::NativeText));
        item_shortcut->setFlags(item_shortcut->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 2, item_shortcut);
    }
}

void ShortcutEditor::onEditShortcut(int row, [[maybe_unused]] int column) {

    QString action_id = m_table->item(row, 0)->text();
    if (!m_actions.contains(action_id)) {
        return;
    }

    App::ShortcutAction& act = m_actions[action_id];

    // edit shortcut dialog
    QDialog dialog(this);
    dialog.setWindowTitle("Edit Shortcut");

    auto* layout = new QVBoxLayout(&dialog);

    auto* label = new QLabel("Press the key combination you want to assign.\nPress Esc to clear the shortcut.");
    layout->addWidget(label);

    auto* key_edit = new QKeySequenceEdit(&dialog);
    key_edit->setKeySequence(act.action->shortcut());
    layout->addWidget(key_edit);

    auto* button_layout = new QHBoxLayout();
    auto* btn_ok        = new QPushButton("Ok");
    auto* btn_cancel    = new QPushButton("Cancel");
    auto* btn_clear     = new QPushButton("Clear");

    connect(btn_ok, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(btn_cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(btn_clear, &QPushButton::clicked, [key_edit]() { key_edit->clear(); });

    button_layout->addWidget(btn_clear);
    button_layout->addStretch();
    button_layout->addWidget(btn_ok);
    button_layout->addWidget(btn_cancel);
    layout->addLayout(button_layout);

    auto* filter = new KeySequenceEditFilter(&dialog);
    key_edit->installEventFilter(filter);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QKeySequence newShortcut = key_edit->keySequence();

    // check for conflicts
    for (auto it = m_actions.constBegin(); it != m_actions.constEnd(); ++it) {
        if (it.key() != action_id && it.value().action->shortcut() == newShortcut && !newShortcut.isEmpty()) {
            QMessageBox::warning(
                this,
                "Shortcut Conflict",
                QString("This shortcut is already assigned to '%1'.\nPlease choose a different shortcut.")
                    .arg(it.value().description)
            );
            return;
        }
    }

    // update the action and table
    act.action->setShortcut(newShortcut);
    m_table->item(row, 2)->setText(newShortcut.toString(QKeySequence::NativeText));
}

void ShortcutEditor::onResetToDefaults() {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Reset Shortcuts",
        "Are you sure you want to reset all shortcuts to their default values?",
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        for (auto it = m_actions.begin(); it != m_actions.end(); ++it) {
            it.value().action->setShortcut(it.value().default_shortcut);
        }
        populateTable();
    }
}

}
