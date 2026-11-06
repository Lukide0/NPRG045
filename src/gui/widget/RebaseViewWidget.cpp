#include "gui/widget/RebaseViewWidget.h"

#include "App.h"
#include "app/conflict/ConflictEditorManager.h"
#include "app/conflict/ConflictTreeModel.h"
#include "app/rebase/RebaseController.h"
#include "app/rebase/RebasePlanModel.h"
#include "core/action/Action.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/GitGraph.h"
#include "gui/error.h"
#include "gui/style/GlobalStyle.h"
#include "gui/style/StyleManager.h"
#include "gui/widget/CommitViewWidget.h"
#include "gui/widget/ConflictWidget.h"
#include "gui/widget/DiffWidget.h"
#include "gui/widget/LineSplitter.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

#include <git2/checkout.h>
#include <git2/cherrypick.h>
#include <git2/commit.h>
#include <git2/errors.h>
#include <git2/index.h>
#include <git2/merge.h>
#include <git2/oid.h>
#include <git2/refs.h>
#include <git2/repository.h>
#include <git2/signature.h>
#include <git2/status.h>
#include <git2/tree.h>
#include <git2/types.h>

#include <QAbstractItemModel>
#include <QBoxLayout>
#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QList>
#include <QListWidgetItem>
#include <QObject>
#include <QPalette>
#include <QPushButton>
#include <QSplitter>
#include <QString>
#include <Qt>
#include <QTimer>
#include <QWidget>

namespace gui::widget {

using core::action::Action;
using core::action::ActionType;

RebaseViewWidget::RebaseViewWidget(QWidget* parent)
    : QWidget(parent)
    , m_graph(GitGraph::empty()) {
    using style::GlobalStyle;

    auto* widget_layout = new QHBoxLayout();
    widget_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(widget_layout);

    auto* horizontal_split    = new LineSplitter(Qt::Orientation::Horizontal);
    auto* left_split          = new LineSplitter(Qt::Orientation::Vertical);
    auto* diff_commit_split   = new LineSplitter(Qt::Orientation::Vertical);
    auto* diff_conflict_split = new LineSplitter(Qt::Orientation::Horizontal);

    auto* right_layout        = new QVBoxLayout();
    auto* right_layout_widget = new QWidget();
    right_layout_widget->setLayout(right_layout);

    horizontal_split->addWidget(left_split);
    horizontal_split->addWidget(right_layout_widget);

    widget_layout->addWidget(horizontal_split);

    auto* graphs_split = new LineSplitter(Qt::Orientation::Horizontal);

    //-- LEFT LAYOUT --------------------------------------------------------//

    //-- actions ---------------------------------------------------------------
    m_list            = new QListView();
    m_action_delegate = new rebase::RebaseActionDelegate(m_list);

    m_list->setItemDelegate(m_action_delegate);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDragEnabled(true);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto palette = m_list->palette();
    palette.setColor(QPalette::Highlight, GlobalStyle::getColor(GlobalStyle::HIGHLIGHT));
    m_list->setPalette(palette);

    left_split->addWidget(m_list);
    left_split->addWidget(graphs_split);

    //-- graphs ----------------------------------------------------------------
    m_graphs.old       = new git::GraphWidget();
    m_graphs.result    = new git::GraphWidget();
    m_graphs.presenter = new git::GraphPresenter(m_graphs.old, m_graphs.result, this);

    graphs_split->addWidget(m_graphs.old);
    graphs_split->addWidget(m_graphs.result);

    //-- RIGHT LAYOUT -------------------------------------------------------//
    m_diff_widget     = new DiffWidget();
    m_commit_view     = new CommitViewWidget(m_diff_widget);
    m_conflict_widget = new ConflictWidget();

    m_conflict_widget->hide();

    right_layout->addWidget(diff_commit_split);

    diff_conflict_split->addWidget(m_diff_widget);
    diff_conflict_split->addWidget(m_conflict_widget);

    diff_commit_split->addWidget(diff_conflict_split);
    diff_commit_split->addWidget(m_commit_view);

    diff_commit_split->setStretchFactor(0, 2);
    diff_commit_split->setStretchFactor(1, 0);

    connect(&style::StyleManager::getGlobalStyle(), &GlobalStyle::changed, this, [this]() {
        auto p = m_list->palette();
        p.setColor(QPalette::Highlight, GlobalStyle::getColor(GlobalStyle::HIGHLIGHT));
        m_list->setPalette(p);
        m_list->viewport()->setPalette(p);
    });

    connect(&style::StyleManager::getConflictStyle(), &style::ConflictStyle::changed, this, [this]() {
        m_list->viewport()->update();
    });

    connect(this, &RebaseViewWidget::currentStepChanged, m_diff_widget, &DiffWidget::setCurrentStep);
    connect(this, &RebaseViewWidget::currentStepChanged, m_commit_view, &CommitViewWidget::setCurrentStep);

    connect(m_graphs.presenter, &git::GraphPresenter::rowRequested, this, [this](int row) {
        m_list->setCurrentIndex(m_controller->model()->index(row, 0));
    });

    connect(m_graphs.presenter, &git::GraphPresenter::commitPicked, this, [this](const QString& id) {
        const auto* node = m_graphs.old->nodeData(id);
        if (node == nullptr || node->commit == nullptr) {
            return;
        }

        m_diff_widget->setCurrentCommit(node->commit);
        m_commit_view->setCurrentCommit(node->commit);
    });

    connect(m_conflict_widget, &ConflictWidget::resolveRequested, this, &RebaseViewWidget::openConflictEditor);

    //-- Conflict editors -----------------------------------------------------/

    m_conflict_editor_manager = new app::conflict::ConflictEditorManager(App::getConflictEditor(), this);

    connect(
        m_conflict_editor_manager,
        &app::conflict::ConflictEditorManager::resolutionReady,
        this,
        &RebaseViewWidget::editConflictResolution
    );

    connect(
        m_conflict_editor_manager,
        &app::conflict::ConflictEditorManager::editorFailed,
        this,
        &RebaseViewWidget::showConflictEditorError
    );
}

void RebaseViewWidget::setSession(app::rebase::RebaseController* controller, GitGraph&& graph) {
    m_list->setModel(nullptr);
    m_controller       = controller;
    m_current_conflict = nullptr;

    if (m_controller == nullptr) {
        return;
    }

    m_list->setModel(m_controller->model());

    // initialize graphs
    m_graph = std::move(graph);
    initializeGraph();

    // update session
    m_diff_widget->setSession(&m_controller->session(), m_controller);

    // update conflict editor manager
    m_conflict_editor_manager->setRepo(&controller->session().repo());

    // initialize
    m_controller->refresh(true);

    connect(m_action_delegate, &rebase::RebaseActionDelegate::editRequested, m_list, [this](const QModelIndex& index) {
        QPersistentModelIndex persistent_index(index);

        QTimer::singleShot(0, m_list, [this, persistent_index] {
            if (persistent_index.isValid()) {
                m_list->edit(persistent_index);
            }
        });
    });

    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex current) {
        changedListSelection(current);
    });

    connect(m_controller, &app::rebase::RebaseController::analysisChanged, this, [this](std::size_t first_changed_row) {
        if (!m_current.isValid() || m_current.row() < static_cast<int>(first_changed_row)) {
            return;
        }

        publishCurrentStep();
    });

    connect(m_commit_view, &CommitViewWidget::commitMessageChanged, this, [this](std::size_t row, const QString& msg) {
        if (msg.isEmpty()) { }

        const auto& analysis = m_controller->analysis();

        if (analysis == nullptr || row >= analysis->steps.size()) {
            return;
        }

        m_controller->setMessage(static_cast<int>(row), msg);
    });

    // pick -> reword does not trigger analysisChanged
    connect(
        m_controller->model(),
        &QAbstractItemModel::dataChanged,
        this,
        [this](const QModelIndex& top, const QModelIndex& bottom, const QList<int>& roles) {
            using app::rebase::RebasePlanModel;

            if (!m_current.isValid() || m_current.row() < top.row() || m_current.row() > bottom.row()) {
                return;
            }

            // an empty list means everything
            const bool relevant = roles.isEmpty() || std::ranges::any_of(roles, [](int r) {
                                      return r == RebasePlanModel::TypeRole || r == RebasePlanModel::MessageRole
                                          || r == RebasePlanModel::CanEditMessageRole
                                          || r == RebasePlanModel::CommitIdRole;
                                  });

            if (relevant) {
                publishCurrentStep();
            }
        }
    );

    // don't change row when splitting or merging actions
    connect(m_controller->model(), &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex&, int first, int) {
        m_list->setCurrentIndex(m_controller->model()->index(first));
    });

    // show
    connect(
        m_controller,
        &app::rebase::RebaseController::errorOccurred,
        this,
        [this](const QString& title, const QString& message) {
            DISPLAY_ERROR(this, title.toStdString(), message.toStdString());
        },
        Qt::QueuedConnection
    );

    // set current index
    m_list->setCurrentIndex(m_controller->model()->index(0));
}

void RebaseViewWidget::initializeGraph() {
    m_graphs.presenter->setPlan(m_controller->model(), m_list->selectionModel());

    QList<git::GraphNodeData> nodes;

    std::uint32_t max_depth = m_graph.max_depth();
    m_graph.reverse_iterate([&](std::uint32_t depth, std::span<core::git::GitNode<void>> level) {
        auto row = static_cast<int>(max_depth - depth);

        assert(level.size() == 1);

        for (auto& node : level) {
            git::GraphNodeData commit;

            const auto* oid = git_commit_id(node.commit);
            commit.row      = row;
            commit.id       = QString::fromStdString(core::git::format_oid_to_str<core::git::OID_SIZE>(oid));
            commit.hash     = commit.id.left(7);
            commit.message  = git_commit_summary(node.commit);
            commit.tooltip  = commit.hash + ' ' + commit.message;
            commit.commit   = node.commit;

            nodes.push_back(commit);
        }
    });

    m_graphs.presenter->setOriginal(nodes);
}

void RebaseViewWidget::changedListSelection(const QModelIndex& index) {
    m_current = index;
    publishCurrentStep();
}

void RebaseViewWidget::publishCurrentStep() {
    const int row       = m_current.isValid() ? m_current.row() : -1;
    const auto analysis = m_controller->analysis();

    updateConflict(row);

    emit currentStepChanged(analysis, row);
}

void RebaseViewWidget::updateConflict(int row) {
    const auto& analysis = m_controller->analysis();

    m_diff_widget->show();
    m_conflict_widget->hide();

    const auto* block = analysis->blocking();
    if (row < 0 || row >= static_cast<int>(analysis->steps.size()) || block == nullptr
        || static_cast<int>(block->index) != row) {
        return;
    }

    m_conflict_widget->show();
    m_conflict_widget->clearConflicts();

    if (!block->has_result()) {
        m_diff_widget->hide();
    }

    auto* conflict = block->conflict.get();

    QSet<app::conflict::ConflictFileId> ids;
    QList<app::conflict::ConflictTreeModel::ConflictFile> files;

    ids.reserve(static_cast<int>(conflict->files.size()));
    files.reserve(static_cast<int>(conflict->files.size()));

    for (std::size_t i = 0; i < conflict->files.size(); ++i) {
        const auto& f = conflict->files[i];

        ids.insert(
            app::conflict::ConflictFileId {
                .path  = f.path,
                .entry = f.entry,
            }
        );

        files.push_back(
            app::conflict::ConflictTreeModel::ConflictFile {
                .id       = i,
                .path     = QString::fromStdString(f.path),
                .entry    = f.entry,
                .resolved = f.resolved(),
            }
        );
    }

    m_current_conflict = conflict;
    m_conflict_editor_manager->discardExcept(ids);
    m_conflict_widget->setConflictFiles(files);

    // no change
    if (conflict == m_current_conflict) {
        return;
    }
}

void RebaseViewWidget::openConflictEditor(
    std::size_t id,
    const QString& path,
    const core::conflict::ConflictEntry& entry,
    ConflictWidget::ConflictEditMode mode
) {
    if (m_current_conflict == nullptr) {
        return;
    }

    if (id >= m_current_conflict->files.size()) {
        return;
    }

    const auto& file = m_current_conflict->files[id];
    if (file.entry != entry || file.path != path) {
        return;
    }

    bool edit_resolution;
    switch (mode) {
    case ConflictWidget::ConflictEditMode::Resolve:
        edit_resolution = false;
        break;
    case ConflictWidget::ConflictEditMode::EditResolution:
        edit_resolution = true;
        break;
    }

    auto file_id = app::conflict::ConflictFileId { .path = file.path, .entry = file.entry };
    auto editor  = m_conflict_editor_manager->open(file_id, file, edit_resolution);
    if (!editor.has_value()) {
        auto err = editor.error();
        DISPLAY_ERROR(this, err.title, err.message);
    }
}

void RebaseViewWidget::editConflictResolution(
    app::conflict::ConflictFileId id, const QByteArray& content, app::conflict::ConflictResolutionType type
) {
    if (m_current_conflict == nullptr) {
        return;
    }

    switch (type) {
    case app::conflict::ConflictResolutionType::Deleted:
        m_controller->setResolutionDeleted(id.entry);
        break;

    case app::conflict::ConflictResolutionType::Modified: {
        auto resolution = m_controller->setResolution(id.entry, content);
        if (!resolution.has_value()) {
            const auto& err = resolution.error();
            DISPLAY_ERROR(this, err.title, err.message);
            return;
        }
        break;
    }
    }
}

void RebaseViewWidget::showConflictEditorError(app::conflict::ConflictFileId id, QString message) {
    if (m_current_conflict == nullptr) {
        return;
    }

    const QString path = QString::fromStdString(id.path);
    DISPLAY_ERROR(this, "Conflict editor error", QStringLiteral("Could not edit \"%1\".\n\n%2").arg(path).arg(message));
}

void RebaseViewWidget::moveSelectedAction(bool down) {
    auto* model = m_list->model();
    if (model == nullptr) {
        return;
    }

    const int row_count = model->rowCount();
    if (row_count == 0) {
        return;
    }

    if (!m_current.isValid()) {
        const QModelIndex first = model->index(0, 0);

        m_list->selectionModel()->setCurrentIndex(
            first, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows
        );

        m_list->scrollTo(first);
        return;
    }

    const int from = m_current.row();
    if (from < 0 || from >= row_count) {
        return;
    }

    const int to = down ? from + 1 : from - 1;
    if (to < 0 || to >= row_count) {
        return;
    }

    m_controller->moveAction(from, to);
    const QModelIndex target = model->index(to, 0);

    m_list->selectionModel()->setCurrentIndex(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_list->scrollTo(target);
}

void RebaseViewWidget::changeActionType() {
    if (!m_current.isValid()) {
        return;
    }

    const int current_type = m_current.data(app::rebase::RebasePlanModel::TypeRole).toInt();
    const auto& types      = core::action::action_types;

    const auto* it = std::ranges::find_if(types, [=](auto t) { return static_cast<int>(t) == current_type; });

    if (it == types.end()) {
        return;
    }

    const auto index      = std::distance(types.begin(), it);
    const auto next_index = (index + 1) % types.size();

    const auto new_type = types[next_index];

    m_list->model()->setData(m_current, static_cast<int>(new_type), app::rebase::RebasePlanModel::TypeRole);
}

void RebaseViewWidget::changeActionType(ActionType type) {

    if (!m_current.isValid()) {
        return;
    }

    const int old_type = m_current.data(app::rebase::RebasePlanModel::TypeRole).toInt();
    const int new_type = static_cast<int>(type);

    if (old_type == new_type) {
        return;
    }

    m_list->model()->setData(m_current, new_type, app::rebase::RebasePlanModel::TypeRole);
}

}
