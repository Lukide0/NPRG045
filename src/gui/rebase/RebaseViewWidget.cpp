#include "gui/rebase/RebaseViewWidget.h"

#include "App.h"
#include "app/conflict/ConflictCheckout.h"
#include "app/conflict/ConflictEditorManager.h"
#include "app/conflict/ConflictTreeModel.h"
#include "app/rebase/RebaseController.h"
#include "app/rebase/RebasePlanModel.h"
#include "core/action/Action.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/error.h"
#include "core/git/GitGraph.h"
#include "gui/error.h"
#include "gui/git/CommitViewWidget.h"
#include "gui/git/ConflictWidget.h"
#include "gui/git/DiffWidget.h"
#include "gui/style/GlobalStyle.h"
#include "gui/style/StyleManager.h"
#include "gui/widget/LineSplitter.h"
#include "gui/widget/ScrollListView.h"

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
#include <QDesktopServices>
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

namespace gui::rebase {

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
    m_list            = new ScrollListView();
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
    m_diff_widget     = new git::DiffWidget();
    m_commit_view     = new git::CommitViewWidget(m_diff_widget);
    m_conflict_widget = new git::ConflictWidget();

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

    connect(this, &RebaseViewWidget::currentStepChanged, m_diff_widget, &git::DiffWidget::setCurrentStep);
    connect(this, &RebaseViewWidget::currentStepChanged, m_commit_view, &git::CommitViewWidget::setCurrentStep);

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

    connect(
        m_conflict_widget, &git::ConflictWidget::resolutionStartRequested, this, &RebaseViewWidget::resolutionStart
    );
    connect(m_conflict_widget, &git::ConflictWidget::resolutionSaveRequested, this, &RebaseViewWidget::resolutionSave);

    connect(m_conflict_widget, &git::ConflictWidget::resolveRequested, this, &RebaseViewWidget::openConflictEditor);

    connect(m_conflict_widget, &git::ConflictWidget::resolutionCancelRequested, this, [this]() {
        resolutionCancel(/*ask=*/true);
    });

    connect(m_conflict_widget, &git::ConflictWidget::openDirectoryRequested, this, [this]() {
        if (!m_conflict_editor_manager->isOpenResolution()) {
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_conflict_editor_manager->workdir()));
    });

    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState s) {
        if (s == Qt::ApplicationActive && m_conflict_editor_manager != nullptr
            && m_conflict_editor_manager->isOpenResolution()) {
            updateConflicts();
            m_conflict_widget->updateButtons();
        }
    });
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

    // connect conflict editor manager
    m_conflict_editor_manager = m_controller->editors();
    m_conflict_editor_manager->setRepo(&controller->session().repo());

    connect(
        m_conflict_editor_manager,
        &app::conflict::ConflictEditorManager::editorClosed,
        this,
        &RebaseViewWidget::editConflictResolution
    );

    connect(
        m_conflict_editor_manager,
        &app::conflict::ConflictEditorManager::editorFailed,
        this,
        &RebaseViewWidget::showConflictEditorError
    );

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
        syncConflictEditors();

        if (!m_current.isValid() || m_current.row() < static_cast<int>(first_changed_row)) {
            return;
        }

        publishCurrentStep();
    });

    connect(
        m_commit_view, &git::CommitViewWidget::commitMessageChanged, this, [this](std::size_t row, const QString& msg) {
            if (msg.isEmpty()) { }

            const auto& analysis = m_controller->analysis();

            if (analysis == nullptr || row >= analysis->steps.size()) {
                return;
            }

            m_controller->setMessage(static_cast<int>(row), msg);
        }
    );

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

    connect(
        m_controller,
        &app::rebase::RebaseController::blockedByOpenResolution,
        this,
        [this](app::rebase::RebaseController::BlockType type, QList<app::conflict::ConflictFileId> editors) {
            QMessageBox box(this);
            box.setIcon(QMessageBox::Warning);

            const bool blocked_by_resolution = type == app::rebase::RebaseController::BlockType::OpenConflictResolution;

            if (editors.empty() || blocked_by_resolution) {
                box.setWindowTitle("Conflict resolution open");
                box.setText("Discard conflict resolution?");

            } else {
                QStringList names;
                names.reserve(editors.size());

                for (const auto& id : editors) {
                    names << QString::fromStdString(id.path);
                }

                box.setWindowTitle("Conflict editors open");
                box.setText(QStringLiteral("%1 conflict editor(s) still open:").arg(names.size()));
                box.setInformativeText(
                    QStringLiteral("• ") + names.join(QStringLiteral("\n• "))
                    + "\n\nClose them and discard their changes?"
                );
            }

            auto* discard = box.addButton("Discard", QMessageBox::DestructiveRole);
            auto* cancel  = box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(cancel);
            box.setEscapeButton(cancel);

            box.exec();

            if (box.clickedButton() == discard) {
                m_controller->discardEditorsAndContinue();

                if (blocked_by_resolution) {
                    m_conflict_widget->discardResolution();

                    auto restore = m_conflict_editor_manager->resolutionEnd();
                    if (!restore.has_value()) {
                        const auto err = restore.error();
                        DISPLAY_ERROR(this, err.title, err.message);
                    }
                }

            } else {
                m_controller->cancelPending();
            }
        }
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

void RebaseViewWidget::syncConflictEditors() {
    m_conflict_editor_manager->discardExcept(m_controller->currentConflictFiles());
}

void RebaseViewWidget::updateConflict(int row) {
    const auto& analysis = m_controller->analysis();

    m_diff_widget->show();
    m_conflict_widget->hide();

    if (row < 0 || row >= static_cast<int>(analysis->steps.size())) {
        return;
    }

    const auto& step = analysis->steps[row];
    if (step.conflict == nullptr) {
        return;
    }

    switch (step.status) {
    case core::conflict::ConflictState::Unknown:
    case core::conflict::ConflictState::Error:
    case core::conflict::ConflictState::Clean:
        return;
    case core::conflict::ConflictState::Conflict:
    case core::conflict::ConflictState::Resolved:
        break;
    }

    m_conflict_widget->show();

    if (!step.has_result()) {
        m_diff_widget->hide();
    }

    // old conflict
    if (m_conflict_editor_manager->isOpenResolution()) {
        return;
    }

    // new conflict
    m_conflict_widget->clearConflicts();

    auto* conflict = step.conflict.get();

    QList<app::conflict::ConflictTreeModel::ConflictFile> files;

    files.reserve(static_cast<int>(conflict->files.size()));

    for (std::size_t i = 0; i < conflict->files.size(); ++i) {
        const auto& f = conflict->files[i];

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
    m_conflict_widget->setConflictFiles(files);
}

void RebaseViewWidget::updateConflicts() {
    m_conflict_editor_manager->iterateFiles(
        [this](const core::rebase::ConflictFile& file, app::conflict::FileState state) {
            m_conflict_widget->setResolved(file.path, state != app::conflict::FileState::Unresolved);
        }
    );
}

void RebaseViewWidget::resolutionStart() {
    const auto* block = m_controller->analysis()->blocking();
    if (block == nullptr || m_current_conflict == nullptr || m_conflict_editor_manager->isOpenResolution()) {
        m_conflict_widget->onStartFinished(false);
        return;
    }

    auto& conflict                                = *m_current_conflict;
    std::vector<core::rebase::ConflictFile> files = conflict.files;

    auto* commit = block->action->get_commit();

    LOG_INFO("Conflict checkout");
    const auto result = m_conflict_editor_manager->resolutionOpen(conflict.conflict_index, commit, std::move(files));
    if (!result.has_value()) {
        const auto err = result.error();
        DISPLAY_ERROR(this, err.title, err.message);
    }

    m_conflict_widget->onStartFinished(result.has_value());
}

void RebaseViewWidget::resolutionCancel(bool ask) {
    if (!m_conflict_editor_manager->isOpenResolution()) {
        return;
    }

    if (ask) {
        const auto answer = QMessageBox::question(
            this,
            "Cancel conflict resolution",
            "Your edits to the conflict files will be discarded. Continue?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    for (const auto& file : m_conflict_editor_manager->files()) {
        m_conflict_widget->setResolved(file.path, file.resolved());
    }

    m_conflict_editor_manager->discardAll();

    m_conflict_widget->discardResolution();

    auto r = m_conflict_editor_manager->resolutionEnd();
    if (!r.has_value()) {
        const auto err = r.error();
        DISPLAY_ERROR(this, err.title, err.message);
    }
}

void RebaseViewWidget::editConflictResolution(app::conflict::ConflictFileId id, app::conflict::FileState type) {
    if (m_current_conflict == nullptr) {
        return;
    }

    bool is_resolved;
    switch (type) {
    case app::conflict::FileState::Unresolved:
        is_resolved = false;
        break;
    case app::conflict::FileState::Resolved:
    case app::conflict::FileState::ResolvedPending:
    case app::conflict::FileState::ModifiedPending:
    case app::conflict::FileState::DeletedPending:
        is_resolved = true;
        break;
    }

    m_conflict_widget->setResolved(id.path, is_resolved);
}

void RebaseViewWidget::resolutionSave() {
    using app::conflict::PendingResolution;

    const auto* step = m_controller->analysis()->blocking();
    if (m_current_conflict == nullptr || !m_conflict_editor_manager->isOpenResolution() || step == nullptr) {
        return;
    }

    if (m_conflict_editor_manager->hasOpenEditors()) {
        DISPLAY_ERROR(this, "Could not save resolutions", "Close the open editors first");
        return;
    }

    auto foreign = m_conflict_editor_manager->foreignChanges();
    if (!foreign.has_value()) {
        const auto err = foreign.error();
        DISPLAY_ERROR(this, err.title, err.message);
        return;
    }

    if (!foreign->empty()) {
        const auto answer = QMessageBox::warning(
            this,
            "Other files changed",
            QStringLiteral(
                "%1 file(s) outside the conflict were changed in the worktree and will be part of the "
                "result. Continue?"
            )
                .arg(foreign->size()),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
        );

        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    auto state = m_conflict_editor_manager->collectResolutions();
    if (!state.has_value()) {
        const auto err = state.error();
        DISPLAY_ERROR(this, err.title, err.message);
        return;
    }

    git_index* index = state->index;
    if (index == nullptr) {
        DISPLAY_ERROR(this, "Could not save resolutions", "The repository index is not available");
        return;
    }

    if (git_index_has_conflicts(index) != 0) {
        DISPLAY_ERROR(
            this,
            "Could not save resolutions",
            "Please resolve conflicts in the workspace and stage the files using 'git add' or 'git rm'"
        );
        return;
    }

    git_oid tree_oid;
    auto* repo = m_controller->session().repo().repo();
    if (git_index_write_tree_to(&tree_oid, index, repo) != 0) {
        DISPLAY_ERROR(this, "Could not create resolution tree from index", core::git::get_last_error());
        return;
    }

    core::git::tree_t tree;
    if (git_tree_lookup(&tree, repo, &tree_oid) != 0) {
        DISPLAY_ERROR(this, "Could not look up generated resolution tree", core::git::get_last_error());
        return;
    }

    auto conflict = core::conflict::ConflictTrees::create(&step->parent_tree_id, &step->action->get_oid());

    const bool has_file_resolutions = std::ranges::any_of(state->resolutions, [](const PendingResolution& r) {
        return r.kind != PendingResolution::Kind::Unchanged;
    });

    bool save_file_resolutions = false;
    if (has_file_resolutions) {
        const auto answer = QMessageBox::question(
            this,
            "Save file resolutions",
            "Also save the resolution of each file?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );

        save_file_resolutions = answer == QMessageBox::Yes;
    }

    LOG_INFO("Saving resolution");

    auto* stack = m_controller->undoStack();
    stack->beginMacro("Conflict files resolution");

    if (save_file_resolutions) {
        for (const auto& file : state->resolutions) {
            switch (file.kind) {
            case PendingResolution::Kind::Unchanged:
                break;

            case PendingResolution::Kind::Deleted:
                m_controller->setEditorResolution(file.id);
                break;

            case PendingResolution::Kind::Contents: {
                const auto r = m_controller->setEditorResolution(file.id, file.contents);
                if (!r.has_value()) {
                    const auto err = r.error();
                    DISPLAY_ERROR(this, err.title, err.message);
                }
                break;
            }
            }
        }
    }

    m_controller->resolveConflict(conflict, std::move(tree));

    stack->endMacro();

    m_conflict_widget->discardResolution();

    const auto restored = m_conflict_editor_manager->resolutionEnd();
    if (!restored.has_value()) {
        const auto err = restored.error();
        DISPLAY_ERROR(this, err.title, err.message);
        return;
    }
}

void RebaseViewWidget::openConflictEditor(
    std::size_t id,
    const QString& path,
    const core::conflict::ConflictEntry& entry,
    git::ConflictWidget::ConflictEditMode /*unused*/
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

    auto file_id = app::conflict::ConflictFileId { .path = file.path, .entry = file.entry };
    auto editor  = m_conflict_editor_manager->open(file_id);
    if (!editor.has_value()) {
        auto err = editor.error();
        DISPLAY_ERROR(this, err.title, err.message);
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
