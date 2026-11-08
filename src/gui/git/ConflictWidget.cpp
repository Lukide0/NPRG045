#include "gui/git/ConflictWidget.h"

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

    m_button.widget = new QToolButton();
    m_button.widget->setPopupMode(QToolButton::MenuButtonPopup);

    auto* menu = new QMenu(m_button.widget);

    m_button.resolve_conflict = menu->addAction("Resolve original conflict");
    m_button.edit_resolution  = menu->addAction("Edit resolution");

    m_button.widget->setMenu(menu);
    m_button.widget->setDefaultAction(m_button.resolve_conflict);
    m_button.widget->setEnabled(false);

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

    auto* layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(header);
    layout->addWidget(m_button.widget);
    layout->addWidget(m_view);
    layout->addWidget(m_status_bar);
    setLayout(layout);

    connect(m_model, &ConflictTreeModel::countsChanged, this, [this](int unresolved, int total) {
        updateSummary(unresolved, total);
    });

    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current) {
        showSelection(current);
    });

    connect(m_button.resolve_conflict, &QAction::triggered, this, [this]() {
        resolveConflict(ConflictEditMode::Resolve);
    });

    connect(m_button.edit_resolution, &QAction::triggered, this, [this]() {
        resolveConflict(ConflictEditMode::EditResolution);
    });

    updateSummary(0, 0);
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
    m_model->clear();
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
    const QString path        = m_model->pathAt(current);
    const bool is_file        = !path.isEmpty();
    const bool has_resolution = is_file && m_model->isResolved(current);

    m_status_bar->clearMessage();

    if (is_file) {
        m_status_bar->showMessage(path);
    }

    m_button.resolve_conflict->setEnabled(is_file);
    m_button.edit_resolution->setEnabled(has_resolution);

    auto* active = has_resolution ? m_button.edit_resolution : m_button.resolve_conflict;

    m_button.widget->setDefaultAction(active);
    m_button.widget->setEnabled(is_file);
}

}
