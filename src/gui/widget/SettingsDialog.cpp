
#include "gui/widget/SettingsDialog.h"
#include "App.h"
#include "gui/style/StyleManager.h"
#include "gui/widget/ShortcutEditor.h"

#include <qcolor.h>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>

namespace gui::widget {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Preferences");
    setModal(true);
    setMinimumWidth(500);

    m_layout = new QVBoxLayout(this);
    setup();
}

void SettingsDialog::updateButton(QPushButton* btn, const QColor& color) {
    QPalette p = btn->palette();
    p.setColor(QPalette::ButtonText, color);
    btn->setPalette(p);
}

void SettingsDialog::setup() {

    m_tabs = new QTabWidget();

    setupColors();
    setupShortcuts();

    m_layout->addWidget(m_tabs);
    m_layout->addStretch();

    m_button_layout = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    m_apply_button  = m_button_layout->button(QDialogButtonBox::Apply);

    QPushButton* ok_btn = m_button_layout->button(QDialogButtonBox::Ok);
    ok_btn->setText("Apply and close");

    connect(m_button_layout->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &SettingsDialog::onApply);
    connect(m_apply_button, &QPushButton::clicked, this, &SettingsDialog::onApply);
    connect(m_button_layout->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &SettingsDialog::onCancel);
    connect(m_button_layout, &QDialogButtonBox::accepted, this, &QDialog::accept);

    m_layout->addWidget(m_button_layout);
}

void SettingsDialog::setupShortcuts() {

    auto* editor = new ShortcutEditor();
    m_tabs->addTab(editor, "Keyboard shortcuts");
}

void SettingsDialog::setupColors() {
    using style::ConflictStyle;
    using style::DiffStyle;

    auto* tab          = new QWidget();
    auto* color_layout = new QVBoxLayout(tab);

    m_tabs->addTab(tab, "Colors");

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

void SettingsDialog::loadSettings() {
    QSettings settings = App::getSettings();

    auto& style_manager = style::StyleManager::get();
    style_manager.load_styles(settings);

    App::loadShortcuts(settings);
    LOG_INFO("Loading settings");
}

void SettingsDialog::saveSettings() {
    QSettings settings = App::getSettings();

    auto& style_manager = style::StyleManager::get();
    style_manager.save_styles(settings);

    App::saveShortcuts(settings);

    // write to disk
    settings.sync();

    LOG_INFO("Saving settings");
}

void SettingsDialog::applySettings() {
    auto& style_manager = style::StyleManager::get();

    // update styles
    style_manager.diff_style().emit_changed();
    style_manager.conflict_style().emit_changed();
}

void SettingsDialog::onApply() {
    saveSettings();
    applySettings();
}

void SettingsDialog::onCancel() {
    loadSettings();
    applySettings();
    reject();
}

}
