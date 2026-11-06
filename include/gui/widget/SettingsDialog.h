#pragma once

#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QString>
#include <Qt>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::widget {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void onApply();
    void onCancel();
    void onAccept();

private:
    QVBoxLayout* m_layout;
    QTabWidget* m_tabs;
    QDialogButtonBox* m_button_layout;
    QPushButton* m_apply_button;

    struct {
        struct {
            QLineEdit* editor;
            QLabel* error;
            QWidget* tab;
            QComboBox* merge_style = nullptr;
        } conflict;

    } m_tab_data;

    void setup();
    void setupColors();
    void setupShortcuts();
    void setupConflict();

    void loadSettings();
    void saveSettings() const;
    void notifySettingChanged();

    bool applyChanges();

    static void updateButton(QPushButton* btn, const QColor& color, QPalette::ColorRole role = QPalette::ButtonText);

    template <typename T, bool Background = false> QLayout* create_color_picker(const QString& text, T::Style style) {
        constexpr QPalette::ColorRole role = (Background) ? QPalette::Button : QPalette::ButtonText;

        QColor color = T::getColor(style);

        auto* layout = new QHBoxLayout();

        auto* change_btn = new QPushButton(text);
        change_btn->setCursor(Qt::PointingHandCursor);
        change_btn->setMaximumWidth(120);

        auto* load_default_btn = new QPushButton("Reset to default");

        updateButton(change_btn, color, role);

        layout->addWidget(change_btn);
        layout->addWidget(load_default_btn);

        connect(change_btn, &QPushButton::clicked, this, [this, change_btn, color, style]() {
            QColor new_color = QColorDialog::getColor(color, this, "Select color");
            if (!new_color.isValid()) {
                return;
            }

            T::setColor(style, new_color);
            updateButton(change_btn, new_color, role);

            T::emitChanged();
        });

        connect(load_default_btn, &QPushButton::clicked, this, [this, change_btn, style]() {
            QColor default_color = T::getDefaultColor(style);

            T::setColor(style, default_color);
            updateButton(change_btn, default_color, role);

            T::emitChanged();
        });

        return layout;
    }
};

}
