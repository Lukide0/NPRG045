#include "gui/widget/SettingsDialog.h"

#include "App.h"
#include "app/conflict/ConflictEditor.h"
#include "gui/style/ConflictStyle.h"
#include "gui/style/DiffStyle.h"
#include "gui/style/GlobalStyle.h"
#include "gui/style/StyleManager.h"
#include "gui/widget/ShortcutEditor.h"
#include "logging/Log.h"

#include <QColor>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::widget {

bool validate_conflict(QLineEdit* editor, QLabel* error) {
    auto result = app::conflict::ConflictEditor::validate(editor->text());

    QString msg;

    if (result.has_value()) {
        error->clear();
        error->hide();

    } else {
        msg = QString::fromStdString(result.error().message);

        error->setText(msg);
        error->show();
    }

    editor->setProperty("invalid", !result.has_value());
    editor->setToolTip(msg);
    editor->style()->unpolish(editor);
    editor->style()->polish(editor);
    editor->update();

    return result.has_value();
}

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Preferences");
    setModal(true);
    setMinimumWidth(500);

    m_layout = new QVBoxLayout(this);
    setup();
}

void SettingsDialog::updateButton(QPushButton* btn, const QColor& color, QPalette::ColorRole role) {
    QPalette p = btn->palette();
    p.setColor(role, color);
    btn->setPalette(p);
}

void SettingsDialog::setup() {

    m_tabs = new QTabWidget();

    setupColors();
    setupShortcuts();
    setupConflict();

    m_layout->addWidget(m_tabs);
    m_layout->addStretch();

    m_button_layout = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    m_apply_button  = m_button_layout->button(QDialogButtonBox::Apply);

    QPushButton* ok_btn = m_button_layout->button(QDialogButtonBox::Ok);
    ok_btn->setText("Apply and close");

    connect(ok_btn, &QPushButton::clicked, this, &SettingsDialog::onAccept);
    connect(m_apply_button, &QPushButton::clicked, this, &SettingsDialog::onApply);
    connect(m_button_layout, &QDialogButtonBox::rejected, this, &SettingsDialog::onCancel);

    m_layout->addWidget(m_button_layout);
}

void SettingsDialog::setupShortcuts() {

    auto* editor = new ShortcutEditor();
    m_tabs->addTab(editor, "Keyboard shortcuts");
}

void SettingsDialog::setupColors() {
    using style::ConflictStyle;
    using style::DiffStyle;
    using style::GlobalStyle;

    auto* tab          = new QWidget();
    auto* color_layout = new QVBoxLayout(tab);

    m_tabs->addTab(tab, "Colors");

    // global colors
    {
        auto* global_colors_group  = new QGroupBox("Colors");
        auto* global_colors_layout = new QFormLayout(global_colors_group);

        auto* global_highlight = create_color_picker<GlobalStyle, true>("Text", GlobalStyle::HIGHLIGHT);

        global_colors_layout->addRow("Highlight:", global_highlight);
        color_layout->addWidget(global_colors_group);
    }

    // diff colors
    {
        auto* diff_colors_group  = new QGroupBox("Diff Colors");
        auto* diff_colors_layout = new QFormLayout(diff_colors_group);

        // clang-format off
        auto* diff_normal   = create_color_picker<DiffStyle>("Normal line", DiffStyle::NORMAL);
        auto* diff_info     = create_color_picker<DiffStyle>("@@ Header @@", DiffStyle::INFO);
        auto* diff_deletion = create_color_picker<DiffStyle>("- Deletion", DiffStyle::DELETION);
        auto* diff_addition = create_color_picker<DiffStyle>("+ Addition", DiffStyle::ADDITION);
        // clang-format on

        diff_colors_layout->addRow("Normal:", diff_normal);
        diff_colors_layout->addRow("Header:", diff_info);
        diff_colors_layout->addRow("Deletion:", diff_deletion);
        diff_colors_layout->addRow("Addition:", diff_addition);

        color_layout->addWidget(diff_colors_group);
    }

    // conflict colors
    {
        auto* conflict_colors_group  = new QGroupBox("Conflict Colors");
        auto* conflict_colors_layout = new QFormLayout(conflict_colors_group);

        // clang-format off
        auto* conflict_normal   = create_color_picker<ConflictStyle>("Normal", ConflictStyle::NORMAL);
        auto* conflict_conflict = create_color_picker<ConflictStyle>("Conflict", ConflictStyle::CONFLICT);
        auto* conflict_resolved = create_color_picker<ConflictStyle>("Resolved conflict", ConflictStyle::RESOLVED_CONFLICT);
        auto* conflict_unknown  = create_color_picker<ConflictStyle>("Unknown", ConflictStyle::UNKNOWN);
        // clang-format on

        conflict_colors_layout->addRow("Normal:", conflict_normal);
        conflict_colors_layout->addRow("Conflict:", conflict_conflict);
        conflict_colors_layout->addRow("Resolved:", conflict_resolved);
        conflict_colors_layout->addRow("Unknown:", conflict_unknown);

        color_layout->addWidget(conflict_colors_group);
    }
}

void SettingsDialog::setupConflict() {
    using app::conflict::ConflictEditor;

    auto* tab               = new QWidget();
    m_tab_data.conflict.tab = tab;

    m_tabs->addTab(tab, "Conflict");

    auto* conflict_layout = new QVBoxLayout(tab);

    auto* editor_group  = new QGroupBox("Conflict editor", tab);
    auto* editor_layout = new QFormLayout(editor_group);

    auto* editor = new QLineEdit(editor_group);
    editor->setPlaceholderText("editor --base <BASE> --ours <OUR> --theirs <THEIR>");

    auto* error_label = new QLabel(editor);
    error_label->setWordWrap(true);
    error_label->setVisible(false);
    error_label->setProperty("role", "error");

    auto& conflict_editor = App::getConflictEditor();

    auto* description = new QLabel(
        "Available placeholders: <BASE>, <OUR>, <THEIR>, <RESULT>.\n"
        "They are replaced with paths to temporary files.",
        editor_group
    );
    description->setWordWrap(true);

    auto* merge_style = new QComboBox(editor_group);

    merge_style->addItem("Merge (standard)", static_cast<int>(ConflictEditor::ConflictStyle::MERGE));
    merge_style->addItem("Diff3 (includes ancestor)", static_cast<int>(ConflictEditor::ConflictStyle::DIFF3));
    merge_style->addItem("Zdiff3 (compact diff3)", static_cast<int>(ConflictEditor::ConflictStyle::ZDIFF3));
    merge_style->setToolTip("Controls the conflict marker format in the initial merge result.");

    editor_layout->addRow("Command template:", editor);
    editor_layout->addRow(error_label);
    editor_layout->addRow("Conflict marker style:", merge_style);
    editor_layout->addRow(description);

    auto* reset_button = new QPushButton("Reset to default", editor_group);
    editor_layout->addRow(reset_button);

    m_tab_data.conflict.editor      = editor;
    m_tab_data.conflict.error       = error_label;
    m_tab_data.conflict.merge_style = merge_style;

    connect(reset_button, &QPushButton::clicked, this, [editor, merge_style] {
        editor->clear();
        int index = merge_style->findData(static_cast<int>(ConflictEditor::ConflictStyle::ZDIFF3));
        merge_style->setCurrentIndex(index);
    });

    connect(editor, &QLineEdit::editingFinished, this, [editor, error_label, &conflict_editor] {
        if (validate_conflict(editor, error_label)) {
            conflict_editor.setEditor(editor->text());
        }
    });

    connect(merge_style, &QComboBox::currentIndexChanged, this, [merge_style, &conflict_editor](int index) {
        const auto style = static_cast<ConflictEditor::ConflictStyle>(merge_style->itemData(index).toInt());
        conflict_editor.setStyle(style);
    });

    conflict_layout->addWidget(editor_group);
    conflict_layout->addStretch();

    // set values

    editor->setText(conflict_editor.commandTemplate());

    auto index = merge_style->findData(static_cast<int>(conflict_editor.style()));
    if (index < 0) {
        index = merge_style->findData(static_cast<int>(ConflictEditor::ConflictStyle::ZDIFF3));
    }

    merge_style->setCurrentIndex(index);
}

void SettingsDialog::loadSettings() {
    QSettings settings = App::getSettings();

    auto& style_manager = style::StyleManager::get();
    style_manager.loadStyles(settings);

    App::loadShortcuts(settings);
    App::getConflictEditor().load(settings);
    LOG_INFO("Loading settings");
}

void SettingsDialog::saveSettings() const {
    QSettings settings = App::getSettings();

    auto& style_manager = style::StyleManager::get();
    style_manager.saveStyles(settings);

    auto& conflict_editor = App::getConflictEditor();
    conflict_editor.setEditor(m_tab_data.conflict.editor->text());
    conflict_editor.setStyle(
        static_cast<app::conflict::ConflictEditor::ConflictStyle>(
            m_tab_data.conflict.merge_style->currentData().toInt()
        )
    );

    conflict_editor.save(settings);

    App::saveShortcuts(settings);
    // write to disk
    settings.sync();

    LOG_INFO("Saving settings");
}

void SettingsDialog::notifySettingChanged() {
    auto& style_manager = style::StyleManager::get();

    // update styles
    style_manager.diffStyle().emitChangedSignal();
    style_manager.conflictStyle().emitChanged();
    style_manager.globalStyle().emitChangedSignal();
}

void SettingsDialog::onApply() { applyChanges(); }

void SettingsDialog::onCancel() {
    loadSettings();
    notifySettingChanged();

    QDialog::reject();
}

void SettingsDialog::onAccept() {
    if (!applyChanges()) {
        return;
    }

    QDialog::accept();
}

bool SettingsDialog::applyChanges() {
    auto* editor      = m_tab_data.conflict.editor;
    auto* error_label = m_tab_data.conflict.error;

    if (!validate_conflict(editor, error_label)) {
        m_tabs->setCurrentWidget(m_tab_data.conflict.tab);
        editor->setFocus();
        return false;
    }

    saveSettings();
    return true;
}

}
