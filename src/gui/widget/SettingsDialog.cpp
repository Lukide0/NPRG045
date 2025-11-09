
#include "gui/widget/SettingsDialog.h"
#include "gui/style/StyleManager.h"

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

    setup();
}

void update_button(QPushButton* btn, const QColor& color) {
    QPalette p = btn->palette();
    p.setColor(QPalette::ButtonText, color);
    btn->setPalette(p);
}

void SettingsDialog::setup() {
    using style::DiffStyle;

    m_layout = new QVBoxLayout(this);

    // diff colors
    {
        auto* diff_colors_group  = new QGroupBox("Diff Colors");
        auto* diff_colors_layout = new QFormLayout(diff_colors_group);

        auto create_color_picker = [&](const QString& text, DiffStyle::Style style) -> QPushButton* {
            QColor color = DiffStyle::get_color(style);

            auto* btn = new QPushButton(text);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setMaximumWidth(100);

            update_button(btn, color);

            connect(btn, &QPushButton::clicked, this, [btn, this, color, style]() {
                QColor new_color = QColorDialog::getColor(color, this, "Select color");
                if (!new_color.isValid()) {
                    return;
                }

                DiffStyle::set_color(style, new_color);
                update_button(btn, new_color);

                emit diffStyleChanged(style);
            });

            return btn;
        };

        // clang-format off
        auto* diff_normal   = create_color_picker("Normal line", DiffStyle::NORMAL);
        auto* diff_info     = create_color_picker("@@ Header @@", DiffStyle::INFO);
        auto* diff_deletion = create_color_picker("- Deletion", DiffStyle::DELETION);
        auto* diff_addition = create_color_picker("+ Addition", DiffStyle::ADDITION);
        // clang-format on

        diff_colors_layout->addRow("Normal:", diff_normal);
        diff_colors_layout->addRow("Header:", diff_info);
        diff_colors_layout->addRow("Deletion:", diff_deletion);
        diff_colors_layout->addRow("Addition:", diff_addition);

        m_layout->addWidget(diff_colors_group);
    }

    // buttons

    m_layout->addStretch();

    m_button_box = new QDialogButtonBox(
        QDialogButtonBox::Reset | QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply
    );
    m_apply_button = m_button_box->button(QDialogButtonBox::Apply);

    QPushButton* ok_btn = m_button_box->button(QDialogButtonBox::Ok);
    ok_btn->setText("Apply and close");

    connect(m_button_box->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, &SettingsDialog::resetSettings);
    connect(m_button_box->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &SettingsDialog::onApply);
    connect(m_apply_button, &QPushButton::clicked, this, &SettingsDialog::onApply);
    connect(m_button_box->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &SettingsDialog::onCancel);
    connect(m_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);

    m_layout->addWidget(m_button_box);
}

void SettingsDialog::loadSettings() { }

void SettingsDialog::saveSettings() { }

void SettingsDialog::applySettings() { }

void SettingsDialog::resetSettings() {
    loadSettings();
    applySettings();
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

void SettingsDialog::onChange() { applySettings(); }

}
