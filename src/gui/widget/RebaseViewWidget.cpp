#include "gui/widget/RebaseViewWidget.h"

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

    auto palette = m_list->palette();
    palette.setColor(QPalette::Highlight, GlobalStyle::get_color(GlobalStyle::HIGHLIGHT));
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

    connect(&style::StyleManager::get_global_style(), &GlobalStyle::changed, this, [this]() {
        auto p = m_list->palette();
        p.setColor(QPalette::Highlight, GlobalStyle::get_color(GlobalStyle::HIGHLIGHT));
        m_list->setPalette(p);
        m_list->viewport()->setPalette(p);
    });

    connect(&style::StyleManager::get_conflict_style(), &style::ConflictStyle::changed, this, [this]() {
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
}

void RebaseViewWidget::setSession(app::rebase::RebaseController* controller, GitGraph&& graph) {
    m_list->setModel(nullptr);
    m_controller = controller;

    if (m_controller == nullptr) {
        return;
    }

    m_list->setModel(m_controller->model());

    // initialize graphs
    m_graph = std::move(graph);
    initializeGraph();

    // update session
    m_diff_widget->setSession(&m_controller->session(), m_controller);

    // initialize
    m_controller->refresh(true);

    connect(m_action_delegate, &rebase::RebaseActionDelegate::editRequested, m_list, [this](const QModelIndex& index) {
        m_list->edit(index);
    });

    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex current) {
        changedListSelection(current);
    });

    connect(m_controller, &app::rebase::RebaseController::analysisChanged, this, [this](std::size_t first_changed_row) {
        if (!m_current.isValid() || m_current.row() >= static_cast<int>(first_changed_row)) {
            return;
        }

        publishCurrentStep();
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

    auto* conflict = block->conflict.get();

    // no change
    if (conflict == m_current_conflict) {
        m_diff_widget->hide();
        m_conflict_widget->show();
        return;
    }

    m_current_conflict = conflict;

    QList<app::conflict::ConflictTreeModel::ConflictFile> files;
    files.reserve(static_cast<int>(conflict->files.size()));

    for (auto&& file : conflict->files) {
        files.push_back(
            {
                .path     = QString::fromStdString(file.path),
                .entry    = file.entry,
                .resolved = file.resolved(),
            }
        );
    }

    m_conflict_widget->setConflictFiles(files);

    m_diff_widget->hide();
    m_conflict_widget->show();
}

void RebaseViewWidget::openConflictEditor(
    [[maybe_unused]] const QString& path, [[maybe_unused]] const core::conflict::ConflictEntry& entry
) {

    // 1. read the settings and if editor is missing ask for setting one in settings
    // 2. open the editor and when it closes with correct code read and validate the contents and create resolution
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

// void RebaseViewWidget::checkoutAndResolve() {
// // 1. Check if working index is clean
// {
//     git_status_options opts = GIT_STATUS_OPTIONS_INIT;
//
//     git_status_list* list;
//
//     if (git_status_list_new(&list, m_repo, &opts) != 0) {
//         DISPLAY_LIBGIT_ERROR(this, "Status error", core::git::get_last_error());
//         return;
//     }
//
//     bool has_changes = (git_status_list_entrycount(list) != 0);
//     if (has_changes) {
//         DISPLAY_ERROR(this, "Status error", "Working directory is not clean");
//         return;
//     }
// }
//
// LOG_INFO("Working dir is clean");
//
// m_conflict->clear_resolving();
//
// if (git_repository_head(&m_head, m_repo) != 0) {
//     DISPLAY_LIBGIT_ERROR(this, "Repo head error", core::git::get_last_error());
//     return;
// }
//
// LOG_INFO("Saving HEAD: '{}'", git_reference_name(m_head));
//
// // 2. Create temporary commit and checkout onto it
// {
//     Action* parent_act      = m_conflict->parent_action();
//     git_oid const* tree_oid = nullptr;
//     if (parent_act == nullptr) {
//         tree_oid = git_commit_tree_id(m_actions.get_root_commit());
//     } else {
//         tree_oid = git_tree_id(parent_act->get_tree());
//     }
//
//     assert(tree_oid != nullptr);
//
//     core::git::tree_t tree;
//     if (git_tree_lookup(&tree, m_repo, tree_oid) != 0) {
//         DISPLAY_LIBGIT_ERROR(this, "Failed to load tree", core::git::get_last_error());
//         return;
//     }
//
//     core::git::signature_t sig;
//     if (git_signature_default(&sig, m_repo) != 0) {
//         DISPLAY_LIBGIT_ERROR(this, "Failed to load signature", core::git::get_last_error());
//         return;
//     }
//
//     git_oid commit_id;
//     if (git_commit_create(&commit_id, m_repo, nullptr, sig, sig, nullptr, "Tmp commit", tree, 0, nullptr) != 0) {
//         DISPLAY_LIBGIT_ERROR(this, "Failed to create temporary commit", core::git::get_last_error());
//         return;
//     }
//
//     if (!core::git::set_repository_head_detached(m_repo, &commit_id)) {
//         DISPLAY_LIBGIT_ERROR(this, "Repo head error", core::git::get_last_error());
//         return;
//     }
//
//     LOG_INFO("Checking out tree");
// }
//
// // 3. Checkout conflict
// {
//     git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
//
//     opts.checkout_strategy |= GIT_CHECKOUT_SAFE | GIT_CHECKOUT_ALLOW_CONFLICTS | GIT_CHECKOUT_RECREATE_MISSING;
//
//     if (git_checkout_index(m_repo, m_conflict->index(), &opts) != 0) {
//         DISPLAY_LIBGIT_ERROR(this, "Repo head error", core::git::get_last_error());
//         return;
//     }
//
//     LOG_INFO("Checking out index with conflicts");
// }
//
// // 4. Apply conflict resolutions
// {
//     core::git::index_t index;
//     if (git_repository_index(&index, m_repo) != 0) {
//         DISPLAY_LIBGIT_ERROR(this, "Repo error", core::git::get_last_error());
//         return;
//     }
//
//     if (!m_conflict_manager.apply_resolutions(m_conflict->entries(), m_conflict->paths(), m_repo, index.get())) {
//         DISPLAY_LIBGIT_ERROR(this, "Recorded resolution error", core::git::get_last_error());
//         return;
//     }
// }
//
// m_conflict->start_resolving();
//
// // 5. Create dialog to prevent user to modifing application state
// {
//     ConflictDialog dialog(this);
//
//     dialog.exec();
//
//     while (dialog.resolved()) {
//
//         // check if the resolution can be applied
//         if (markResolved()) {
//             break;
//         }
//
//         dialog.exec();
//     }
//
//     // clean working directory
//     if (!dialog.resolved()) {
//         LOG_INFO("Restoring HEAD: '{}'", git_reference_name(m_head));
//
//         if (!core::git::set_repository_head(m_repo, m_head.get())) {
//             DISPLAY_LIBGIT_ERROR(this, "Failed to clean working directory", core::git::get_last_error());
//         }
//     }
// }
// }

// bool RebaseViewWidget::markResolved() {
// if (m_conflict->index() == nullptr || !m_conflict->is_resolving()) {
//     return false;
// }
//
// core::git::index_t repo_index;
// if (git_repository_index(&repo_index, m_repo) != 0) {
//     DISPLAY_LIBGIT_ERROR(this, "Repo index", core::git::get_last_error());
//     return false;
// }
//
// // 1. Add all modified files to the index and create new tree
// auto&& [err, tree_oid] = core::conflict::add_resolved_files(
//     repo_index, m_repo, m_conflict->paths(), m_conflict->entries(), m_conflict_manager
// );
// if (err.has_value()) {
//     DISPLAY_ERROR(this, "Resolution error", *err);
//     return false;
// }
//
// core::git::tree_t tree;
// if (git_tree_lookup(&tree, m_repo, &tree_oid) != 0) {
//     DISPLAY_LIBGIT_ERROR(this, "Tree error", core::git::get_last_error());
//     return false;
// }
//
// LOG_INFO("Restoring HEAD: '{}'", git_reference_name(m_head));
// // Change working tree
// if (!core::git::set_repository_head(m_repo, m_head)) {
//     DISPLAY_LIBGIT_ERROR(this, "Repo head error", core::git::get_last_error());
//     return false;
// }
//
// core::conflict::ConflictTrees conflict;
//
// git_oid const* tree_id;
// if (m_conflict->parent_action() != nullptr) {
//     tree_id = git_tree_id(m_conflict->parent_action()->get_tree());
// } else {
//     git_commit* root_commit = m_actions.get_root_commit();
//     tree_id                 = git_commit_tree_id(root_commit);
// }
//
// conflict.commit_id      = core::git::format_oid_to_str<core::git::OID_SIZE>(&m_conflict->action()->get_oid());
// conflict.parent_tree_id = core::git::format_oid_to_str<core::git::OID_SIZE>(tree_id);
//
// m_conflict_manager.add_trees_resolution(conflict, std::move(tree));
//
// LOG_INFO("Saving conflict resolution");
//
// m_conflict->finish_resolving();
// m_conflict->clear_conflict();
//
// updateConflictList(nullptr);
// updateConflictMarkers();
//
// updateGraph();
//
//     return true;
// }
}
