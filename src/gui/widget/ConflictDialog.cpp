#include "gui/widget/ConflictDialog.h"

namespace gui::widget {

ConflictDialog::ConflictDialog(QWidget* parent)
    : QDialog(parent)
    , m_resolved(false) {

    setWindowTitle("Conflict Resolution");
    setWindowModality(Qt::ApplicationModal);
    setModal(true);

    setupUI();
}

void ConflictDialog::onResolveClicked() {
    m_resolved = true;
    QDialog::accept();
}

void ConflictDialog::onAbortClicked() {
    auto reply = QMessageBox::question(
        this,
        "Abort Operation?",
        "Are you sure you want to abort the operation?\n\n"
        "This will revert all changes from this operation.",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        m_resolved = false;
        QDialog::reject();
    }
}

void ConflictDialog::setupUI() {
    auto* layout = new QVBoxLayout(this);

    auto* message = new QLabel(
        "The operation has checked out conflicts that must be resolved.<br><br>"
        "Please resolve the conflicts, then click 'Mark as Resolved'."
    );
    message->setWordWrap(true);

    layout->addWidget(message);
    layout->addSpacing(20);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();

    m_abortButton = new QPushButton("Abort Operation");
    m_abortButton->setToolTip("Cancel the operation and revert changes");
    connect(m_abortButton, &QPushButton::clicked, this, &ConflictDialog::onAbortClicked);
    buttons->addWidget(m_abortButton);

    m_resolveButton = new QPushButton("Mark as Resolved");
    m_resolveButton->setDefault(true);
    m_resolveButton->setToolTip("Mark conflicts as resolved and continue");
    connect(m_resolveButton, &QPushButton::clicked, this, &ConflictDialog::onResolveClicked);
    buttons->addWidget(m_resolveButton);

    layout->addLayout(buttons);
}

void ConflictDialog::closeEvent(QCloseEvent* event) {
    auto reply = QMessageBox::warning(
        this,
        "Discard Changes?",
        "Closing this dialog without resolving will revert all changes.\n\n"
        "Are you sure you want to discard the changes?",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        m_resolved = false;
        event->accept();
        QDialog::reject();
    } else {
        event->ignore();
    }
}

void ConflictDialog::reject() {
    QCloseEvent event;
    closeEvent(&event);
}

}
