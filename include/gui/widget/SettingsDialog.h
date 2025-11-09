#pragma once

#include "gui/style/DiffStyle.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>

namespace gui::widget {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

signals:
    void diffStyleChanged(style::DiffStyle::Style style);

private slots:
    void onApply();
    void onCancel();

    void onChange();

private:
    void setup();

    void loadSettings();
    void saveSettings();
    void applySettings();
    void resetSettings();

    QVBoxLayout* m_layout;
    QDialogButtonBox* m_button_box;
    QPushButton* m_apply_button;
};

}
