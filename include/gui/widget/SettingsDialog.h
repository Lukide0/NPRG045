#pragma once

#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace gui::widget {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void onApply();
    void onCancel();

private:
    QVBoxLayout* m_layout;
    QTabWidget* m_tabs;
    QDialogButtonBox* m_button_layout;
    QPushButton* m_apply_button;

    void setup();
    void setupColors();
    void setupShortcuts();

    void loadSettings();
    void saveSettings();
    void applySettings();

    static void updateButton(QPushButton* btn, const QColor& color);

    template <typename T> QLayout* create_color_picker(const QString& text, T::Style style) {
        QColor color = T::get_color(style);

        auto* layout = new QHBoxLayout();

        auto* change_btn = new QPushButton(text);
        change_btn->setCursor(Qt::PointingHandCursor);
        change_btn->setMaximumWidth(120);

        auto* load_default_btn = new QPushButton("Reset to default");

        updateButton(change_btn, color);

        layout->addWidget(change_btn);
        layout->addWidget(load_default_btn);

        connect(change_btn, &QPushButton::clicked, this, [this, change_btn, color, style]() {
            QColor new_color = QColorDialog::getColor(color, this, "Select color");
            if (!new_color.isValid()) {
                return;
            }

            T::set_color(style, new_color);
            updateButton(change_btn, new_color);

            T::emit_changed_signal();
        });

        connect(load_default_btn, &QPushButton::clicked, this, [this, change_btn, style]() {
            QColor default_color = T::get_default_color(style);

            T::set_color(style, default_color);
            updateButton(change_btn, default_color);

            T::emit_changed_signal();
        });

        return layout;
    }
};

}
