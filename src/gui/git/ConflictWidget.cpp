#include "gui/git/ConflictWidget.h"
#include "logging/Log.h"

#include <string>

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QToolButton>
#include <QWidget>

namespace gui::git {

ConflictWidget::ConflictWidget(QWidget* parent)
    : QWidget(parent) {

    using app::conflict::ConflictTreeModel;

    auto* title = new QLabel("Conflicts");

    QFont font = title->font();
    font.setBold(true);
    font.setPointSize(12);
    title->setFont(font);

    m_summary = new QLabel();
    m_summary->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_summary);

    m_buttons.open_directory = new QPushButton("Open conflict workspace");
    m_buttons.open_file      = new QPushButton("Open file conflict");

    m_buttons.open_file->setEnabled(false);
    m_buttons.open_directory->setEnabled(false);

    m_save_button.widget = new QToolButton();
    m_save_button.widget->setPopupMode(QToolButton::MenuButtonPopup);
    {
        auto* menu = new QMenu(m_save_button.widget);

        m_save_button.start  = menu->addAction("Start");
        m_save_button.save   = menu->addAction("Save");
        m_save_button.cancel = menu->addAction("Cancel");

        m_save_button.save->setEnabled(false);
        m_save_button.cancel->setEnabled(false);

        m_save_button.widget->setMenu(menu);
        m_save_button.widget->setDefaultAction(m_save_button.start);
    }
    m_model = new ConflictTreeModel(this);

    m_view = new QTreeView();
    m_view->setModel(m_model);
    m_view->setUniformRowHeights(true);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);

    auto* view_header = m_view->header();

    view_header->setSectionResizeMode(ConflictTreeModel::NameColumn, QHeaderView::Stretch);
    view_header->setSectionResizeMode(ConflictTreeModel::StatusColumn, QHeaderView::ResizeToContents);
    view_header->setStretchLastSection(false);

    // keeps new directories open
    connect(
        m_model, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex& parent_index, int first, int last) {
            for (int row = first; row <= last; ++row) {
                m_view->expandRecursively(m_model->index(row, 0, parent_index));
            }
        }
    );

    m_status_count = new QLabel();
    m_status_bar   = new QStatusBar();
    m_status_bar->setSizeGripEnabled(false);
    m_status_bar->addPermanentWidget(m_status_count);

    auto* actions_layout = new QHBoxLayout();
    actions_layout->setContentsMargins(0, 0, 0, 0);
    actions_layout->setSpacing(6);
    actions_layout->addStretch();
    actions_layout->addWidget(m_save_button.widget);
    actions_layout->addWidget(m_buttons.open_directory);
    actions_layout->addWidget(m_buttons.open_file);

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(header);
    layout->addLayout(actions_layout);
    layout->addWidget(m_view);
    layout->addWidget(m_status_bar);
    setLayout(layout);

    connect(m_model, &ConflictTreeModel::countsChanged, this, [this](int unresolved, int total) {
        updateSummary(unresolved, total);
    });

    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current) {
        showSelection(current);
    });

    connect(m_buttons.open_file, &QPushButton::clicked, this, [this]() {
        if (!m_started) {
            return;
        }
        resolveConflict(ConflictEditMode::Resolve);

        updateButtons();
    });

    connect(m_buttons.open_directory, &QPushButton::clicked, this, [this]() {
        if (!m_started) {
            return;
        }

        emit openDirectoryRequested();
    });

    connect(m_save_button.start, &QAction::triggered, this, [this]() {
        if (m_started || m_start_pending) {
            return;
        }

        m_start_pending = true;
        updateButtons();

        emit resolutionStartRequested();
    });

    connect(m_save_button.save, &QAction::triggered, this, [this]() {
        if (!m_started) {
            return;
        }

        emit resolutionSaveRequested();
        updateButtons();
    });

    connect(m_save_button.cancel, &QAction::triggered, this, [this]() {
        if (!m_started) {
            return;
        }

        emit resolutionCancelRequested();
        updateButtons();
    });

    updateSummary(0, 0);
}

void ConflictWidget::onStartFinished(bool success) {
    m_start_pending = false;

    if (success) {
        m_started = true;
    }

    updateButtons();
}

void ConflictWidget::discardResolution() {
    m_start_pending = false;
    m_started       = false;

    updateButtons();
}

void ConflictWidget::updateButtons() {

    m_save_button.start->setEnabled(!m_started);
    m_save_button.save->setEnabled(m_started);
    m_save_button.cancel->setEnabled(m_started);

    m_save_button.widget->setDefaultAction(m_started ? m_save_button.save : m_save_button.start);

    m_save_button.widget->setEnabled(!m_start_pending);

    m_buttons.open_file->setEnabled(m_started);
    m_buttons.open_directory->setEnabled(m_started);
}

void ConflictWidget::resolveConflict(ConflictEditMode mode) {
    const QModelIndex& idx = m_view->currentIndex();
    if (!idx.isValid()) {
        return;
    }

    const QString path = m_model->pathAt(idx);
    const auto entry   = m_model->entryAt(idx);
    const auto id      = m_model->idAt(idx);

    if (path.isEmpty() || !entry.has_value()) {
        return;
    }

    if (mode == ConflictEditMode::EditResolution && !m_model->isResolved(idx)) {
        return;
    }

    emit resolveRequested(id, path, *entry, mode);
}

void ConflictWidget::setConflictFiles(const ConflictFiles& files) {
    m_status_bar->clearMessage();
    m_model->setPaths(files);

    updateButtons();

    showSelection(m_view->currentIndex());
}

void ConflictWidget::setResolved(const std::string& path, bool resolved) {
    m_model->setResolved(QString::fromStdString(path), resolved);
}

void ConflictWidget::setResolved(const QString& path, bool resolved) { m_model->setResolved(path, resolved); }

void ConflictWidget::setResolvedFiles(const FileList& files) { m_model->setResolvedFiles(files); }

bool ConflictWidget::hasConflict() const { return m_model->unresolvedCount() > 0; }

void ConflictWidget::clearConflicts() {
    m_status_bar->clearMessage();
    m_started       = false;
    m_start_pending = false;
    m_model->clear();

    updateButtons();
}

void ConflictWidget::updateSummary(int unresolved, int total) {
    if (total == 0) {
        m_summary->setText("No conflicts");
        m_status_count->setText(QString());
        return;
    }

    m_summary->setText(QStringLiteral("%1 of %2 resolved").arg(total - unresolved).arg(total));
    m_status_count->setText(QStringLiteral("%1 file(s)").arg(total));
}

void ConflictWidget::showSelection(const QModelIndex& current) {
    const QString path = m_model->pathAt(current);
    const bool is_file = !path.isEmpty();

    m_status_bar->clearMessage();

    if (is_file) {
        m_status_bar->showMessage(path);
    }

    m_buttons.open_file->setEnabled(m_started && is_file);
}

}
