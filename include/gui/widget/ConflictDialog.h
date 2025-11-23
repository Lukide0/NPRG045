#pragma once

#include <QCloseEvent>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::widget {

class ConflictDialog : public QDialog {
    Q_OBJECT

public:
    ConflictDialog(QWidget* parent = nullptr);

    void setResolved(bool resolved) { m_resolved = resolved; }

    [[nodiscard]] bool resolved() const { return m_resolved; }

private slots:

    void onResolveClicked();
    void onAbortClicked();

private:
    bool m_resolved;

    QPushButton* m_resolveButton;
    QPushButton* m_abortButton;

    void setupUI();
    void closeEvent(QCloseEvent* event) override;
    void reject() override;
};

}
