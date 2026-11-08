#include "App.h"

#include "app/Settings.h"
#include "core/action/Action.h"
#include "core/action/Converter.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/commit.h"
#include "core/git/error.h"
#include "core/git/parser.h"
#include "core/git/paths.h"
#include "core/git/RepoSession.h"
#include "core/git/types.h"
#include "gui/error.h"
#include "gui/rebase/RebaseSelectionWidget.h"
#include "gui/rebase/RebaseViewWidget.h"
#include "gui/style/StyleManager.h"
#include "gui/widget/SettingsDialog.h"
#include "gui/widget/WelcomeWidget.h"
#include "logging/Log.h"
#include "state/State.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <git2.h>
#include <git2/branch.h>
#include <git2/commit.h>
#include <git2/errors.h>
#include <git2/global.h>
#include <git2/oid.h>
#include <git2/repository.h>
#include <git2/types.h>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMainWindow>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <qnamespace.h>
#include <QPalette>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStackedLayout>
#include <QStandardPaths>
#include <QString>
#include <QtTypes>

static App* g_app = nullptr;

QSettings App::getSettings() { return app::Settings::get(); }

app::conflict::ConflictEditor& App::getConflictEditor() { return g_app->m_conflict_editor; }

QMap<QString, App::ShortcutAction>& App::getShortcuts() { return g_app->m_shortcuts; }

const core::rebase::RebaseSession* App::rebaseSession() { return g_app->m_rebase.current(); }

void App::loadShortcuts(QSettings& settings) {
    auto& shortcuts = getShortcuts();

    settings.beginGroup("Shortcuts");
    {
        for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
            QString str = settings.value(it.key(), it->default_shortcut.toString()).toString();
            it->action->setShortcut(QKeySequence(str));
        }
    }
    settings.endGroup();
}

void App::saveShortcuts(QSettings& settings) {
    auto& shortcuts = getShortcuts();

    settings.beginGroup("Shortcuts");
    {
        for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
            settings.setValue(it.key(), it->action->shortcut().toString());
        }
    }
    settings.endGroup();
}

void App::registerShortcut(const QString& action_id, QAction* action, const QString& description) {
    ShortcutAction act;
    act.action           = action;
    act.description      = description;
    act.default_shortcut = action->shortcut();

    g_app->m_shortcuts[action_id] = act;
}

App::App() {

    g_app = this;

    connect(qApp, &QApplication::aboutToQuit, [] { LOG_INFO("Application shutting down."); });

    git_libgit2_init();

    QSettings settings = App::getSettings();
    LOG_INFO("User settings: {}", settings.fileName().toStdString());

    // load styles
    {
        auto& manager = gui::style::StyleManager::get();
        manager.loadStyles(settings);
    }

    QPalette palette;
    palette.setColor(QPalette::Window, Qt::white);
    setPalette(palette);

    setup();
    setupShortcuts();

    App::loadShortcuts(settings);
    m_conflict_editor.load(settings);
}

void App::setupShortcuts() {
    auto create_shortcut = [this](
                               const QString& id,
                               QWidget* parent,
                               const QString& desc,
                               Qt::ShortcutContext ctx = Qt::ShortcutContext::WindowShortcut
                           ) -> QAction* {
        auto* action = new QAction(this);
        action->setShortcutContext(ctx);
        parent->addAction(action);

        registerShortcut(id, action, desc);

        return action;
    };

    {
        QListView* actions_list = m_rebase_view->getList();
        auto* actions_move_up   = create_shortcut("actions.move_up", actions_list, "Move focused action up");
        auto* actions_move_down = create_shortcut("actions.move_down", actions_list, "Move focused action down");
        auto* actions_change    = create_shortcut("actions.change_type", actions_list, "Change focused action type");

        for (auto type : core::action::action_types) {
            const char* name = core::action::type_to_str(type);
            QString desc     = QStringLiteral("Change focused action type to '%1'").arg(name);
            QString id       = QStringLiteral("actions.change_%1").arg(name);

            auto* change_type = create_shortcut(id, actions_list, desc);

            connect(change_type, &QAction::triggered, this, [this, type]() { m_rebase_view->changeActionType(type); });
        }

        auto* actions_change_pick
            = create_shortcut("actions.change_pick", actions_list, "Change focused action type to 'pick'");

        connect(actions_move_up, &QAction::triggered, this, [this]() { m_rebase_view->moveActionUp(); });
        connect(actions_move_down, &QAction::triggered, this, [this]() { m_rebase_view->moveActionDown(); });
        connect(actions_change, &QAction::triggered, this, [this]() { m_rebase_view->changeActionType(); });

        connect(actions_change_pick, &QAction::triggered, this, [this]() {
            m_rebase_view->changeActionType(core::action::ActionType::PICK);
        });
    }
}

void App::setup() {
    m_undo_group = new QUndoGroup(this);

    auto* menu = new QMenuBar(this);
    setMenuBar(menu);

    auto* repo  = menu->addMenu("Repo");
    m_repo_open = new QAction(QIcon::fromTheme("folder-open"), "Open", this);

    {
        m_repo_open->setStatusTip("Open a repo");

        connect(m_repo_open, &QAction::triggered, this, [this] {
            if (!m_undo_group->isClean()) {
                auto ans = QMessageBox::question(
                    this,
                    "Unsaved changes",
                    "You have unsaved changes, are you sure you want to open another repo?",
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No
                );

                if (ans == QMessageBox::No) {
                    return;
                }
            }

            openRepoDialog();
        });

        repo->addAction(m_repo_open);
    }

    {
        auto* edit = menu->addMenu("Edit");

        m_history.undo = new QAction("Undo", this);
        m_history.redo = new QAction("Redo", this);

        connect(m_undo_group, &QUndoGroup::activeStackChanged, this, &App::updateUndoActions);
        connect(m_undo_group, &QUndoGroup::canUndoChanged, this, &App::updateUndoActions);
        connect(m_undo_group, &QUndoGroup::canRedoChanged, this, &App::updateUndoActions);
        connect(m_undo_group, &QUndoGroup::undoTextChanged, this, &App::updateUndoActions);
        connect(m_undo_group, &QUndoGroup::redoTextChanged, this, &App::updateUndoActions);

        connect(m_history.undo, &QAction::triggered, this, [this] {
            if (m_controller != nullptr) {
                m_controller->undo();
            }
        });

        connect(m_history.redo, &QAction::triggered, this, [this] {
            if (m_controller != nullptr) {
                m_controller->redo();
            }
        });

        auto* edit_save = new QAction(QIcon::fromTheme("edit-save"), "Save", this);
        connect(edit_save, &QAction::triggered, this, [this] { saveSaveFile(false); });

        auto* edit_save_as = new QAction(QIcon::fromTheme("edit-save"), "Save as", this);
        connect(edit_save_as, &QAction::triggered, this, [this] { saveSaveFile(true); });

        m_load_save = new QAction(QIcon::fromTheme("edit-load"), "Load", this);
        connect(m_load_save, &QAction::triggered, this, [this] { loadSaveFile(); });

        auto* edit_todo_save = new QAction(QIcon::fromTheme("document-save-as"), "Save Todo", this);
        connect(edit_todo_save, &QAction::triggered, this, [this] { saveTodoFile(); });

        auto* edit_preferences = new QAction("Preferences...", this);
        connect(edit_preferences, &QAction::triggered, this, [this] {
            gui::SettingsDialog dialog(this);
            dialog.exec();
        });

        edit->addAction(m_history.undo);
        edit->addAction(m_history.redo);
        edit->addSeparator();
        edit->addAction(edit_save);
        edit->addAction(edit_save_as);
        edit->addAction(m_load_save);
        edit->addAction(edit_todo_save);
        edit->addSeparator();
        edit->addAction(edit_preferences);

        m_history.undo->setShortcut(QKeySequence::Undo);
        m_history.redo->setShortcut(QKeySequence::Redo);
    }
    {
        auto* view             = menu->addMenu("View");
        auto* hide_old_commits = new QAction("Show old commits", this);

        hide_old_commits->setCheckable(true);
        hide_old_commits->setChecked(false);
        connect(hide_old_commits, &QAction::triggered, this, &App::hideOldCommits);

        auto* hide_result_commits = new QAction("Show result commits", this);

        hide_result_commits->setCheckable(true);
        hide_result_commits->setChecked(true);
        connect(hide_result_commits, &QAction::triggered, this, &App::hideResultCommits);

        view->addAction(hide_old_commits);
        view->addAction(hide_result_commits);
    }

    // MAIN ---------------------------------------------------------------
    auto* main = new QWidget(this);
    main->setContentsMargins(0, 0, 0, 0);
    setCentralWidget(main);

    m_layout = new QStackedLayout();
    main->setLayout(m_layout);

    m_rebase_view    = new gui::rebase::RebaseViewWidget();
    m_welcome_widget = new gui::WelcomeWidget();
    m_rebase_select  = new gui::rebase::RebaseSelectionWidget();

    auto* welcome_page   = new QWidget();
    auto* welcome_layout = new QHBoxLayout();
    welcome_page->setLayout(welcome_layout);

    welcome_layout->addWidget(m_welcome_widget);

    m_layout->addWidget(welcome_page);
    m_layout->addWidget(m_rebase_view);
    m_layout->addWidget(m_rebase_select);

    m_rebase_view->hideOldCommits();

    connect(m_welcome_widget, &gui::WelcomeWidget::openCurrentDirectory, this, [this]() {
        QString currentPath = QDir::currentPath();

        // NOTE: openRepo handles widget visibility based on success/failure
        openRepo(currentPath.toStdString());
    });

    connect(m_welcome_widget, &gui::WelcomeWidget::openRepository, this, [this]() {
        // NOTE: openRepoDialog handles widget visibility based on success/failure
        openRepoDialog();
    });

    connect(m_welcome_widget, &gui::WelcomeWidget::loadSaveFile, this, &App::loadSaveFile);

    connect(
        m_rebase_select, &gui::rebase::RebaseSelectionWidget::branchSelectionChanged, this, &App::updateRebaseSelection
    );

    connect(m_rebase_select, &gui::rebase::RebaseSelectionWidget::cancelled, this, [this]() {
        m_layout->setCurrentIndex(page_welcome);
    });

    connect(m_rebase_select, &gui::rebase::RebaseSelectionWidget::completed, this, &App::startNewRebase);
}

void App::updateUndoActions() {
    auto* stack = m_undo_group->activeStack();

    const bool enabled = stack != nullptr;

    m_history.undo->setEnabled(enabled && stack->canUndo());
    m_history.redo->setEnabled(enabled && stack->canRedo());

    m_history.undo->setText(
        (stack != nullptr) && stack->canUndo() ? QStringLiteral("Undo %1").arg(stack->undoText())
                                               : QStringLiteral("Undo")
    );
    m_history.redo->setText(
        (stack != nullptr) && stack->canRedo() ? QStringLiteral("Redo %1").arg(stack->redoText())
                                               : QStringLiteral("Redo")
    );
}

App::SaveStatus App::maybeSave() {
    SaveStatus status = SaveStatus::SAVE;

    if (m_cli_start) {
        QMessageBox msg(this);
        msg.setWindowTitle("Execute Rebase Plan");
        msg.setText("Do you want to execute your rebase plan?");
        msg.setIcon(QMessageBox::Warning);

        auto* save_exec_btn = msg.addButton("Execute", QMessageBox::AcceptRole);
        auto* save_btn      = msg.addButton("Save", QMessageBox::ActionRole);
        auto* discard_btn   = msg.addButton("Discard", QMessageBox::ActionRole);
        auto* cancel_btn    = msg.addButton("Cancel", QMessageBox::RejectRole);

        msg.setDefaultButton(save_exec_btn);
        msg.exec();

        auto* clicked = msg.clickedButton();

        if (clicked == save_exec_btn) {
            if (!saveTodoFile()) {
                status = SaveStatus::CANCEL;
            }
        } else if (clicked == save_btn) {
            if (!saveTodoFile(true)) {
                status = SaveStatus::CANCEL;
            }
        } else if (clicked == discard_btn) {
            status = SaveStatus::DISCARD;
        } else if (clicked == cancel_btn || clicked == nullptr) {
            status = SaveStatus::CANCEL;
        }
    } else if (!m_undo_group->isClean()) {
        auto ans = QMessageBox::warning(
            this,
            "Unsaved changes",
            "Do you want to save your changes?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (ans == QMessageBox::Save) {
            if (!saveSaveFile(false)) {
                status = SaveStatus::CANCEL;
            }
        } else if (ans == QMessageBox::Cancel) {
            status = SaveStatus::CANCEL;
        }
    }

    return status;
}

void App::closeEvent(QCloseEvent* event) {

    switch (maybeSave()) {
    case SaveStatus::SAVE:
        event->accept();
        break;
    case SaveStatus::DISCARD:
        QApplication::exit(1);
        event->accept();
        break;
    case SaveStatus::CANCEL:
        event->ignore();
        break;
    }
}

void App::hideOldCommits(bool state) {
    if (!state) {
        m_rebase_view->hideOldCommits();
    } else {
        m_rebase_view->showOldCommits();
    }
}

void App::hideResultCommits(bool state) {
    if (!state) {
        m_rebase_view->hideResultCommits();
    } else {
        m_rebase_view->showResultCommits();
    }
}

bool App::openRepoDialog() {
    auto folder = QFileDialog::getExistingDirectory(
        this, "Select Repo folder", QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (folder.isEmpty()) {
        return false;
    }

    auto path = folder.toStdString();
    return openRepo(path);
}

void App::openRepoCLI(const std::string& path) {

    std::string repo_path = path;
    if (!std::filesystem::is_directory(path)) {
        repo_path = core::git::repo_path_from_todo(path);
    }

    if (!openRepo(repo_path)) {
        // NOTE: If this method is called before QApplication::exec() then it will do nothing.
        QApplication::exit(1);
        qApp->quit();

        // NOTE: Only called in before QApplication::exec()
        std::exit(1);
    }

    m_cli_start = true;

    m_repo_open->setEnabled(false);
    m_load_save->setEnabled(false);
}

bool App::openRepo(const std::string& path) {
    LOG_INFO("Openning repo: {}", path);

    auto repo = core::git::RepoSession::open(path);
    if (!repo.has_value()) {
        auto err = repo.error();

        DISPLAY_ERROR(this, err.title, err.message);
        m_layout->setCurrentIndex(page_welcome);
        return false;
    }

    auto rebase_info = repo->rebase_info();
    if (!rebase_info.has_value()) {
        auto err = repo.error();

        DISPLAY_ERROR(this, err.title, err.message);
        m_layout->setCurrentIndex(page_welcome);
        return false;
    } else if (!rebase_info->has_value()) {
        LOG_INFO("No rebase is currently in progress");

        if (!prepareRebaseSelection(std::move(*repo))) {
            m_layout->setCurrentIndex(page_welcome);
            return false;
        } else {
            m_layout->setCurrentIndex(page_rebase_select);
            return true;
        }
    }

    auto info = rebase_info.value().value();
    auto err  = openRebase(info, std::move(*repo));
    if (!err.has_value()) {
        auto e = err.error();

        DISPLAY_ERROR(this, e.title, e.message);

        m_layout->setCurrentIndex(page_welcome);
        return false;
    }

    m_layout->setCurrentIndex(page_rebase_view);
    return true;
}

core::Result<void> App::openRebase(const core::git::RebaseInfo& info, core::git::RepoSession&& repo) {
    auto graph = gui::rebase::RebaseViewWidget::GitGraph::create(info.head.c_str(), info.onto.c_str(), repo.repo());
    if (!graph.has_value()) {
        return std::unexpected<core::Error>({
            .title   = "Failed to create git graph",
            .message = "Could not find commits",
        });
    }

    auto plan = core::rebase::RebasePlan::open(repo, info, graph->first_node().commit);
    if (!plan.has_value()) {
        return std::unexpected<core::Error>(plan.error());
    }

    openRebase(std::move(repo), std::move(*plan), { }, std::move(*graph));
    return { };
}

void App::openRebase(
    core::git::RepoSession&& repo,
    core::rebase::RebasePlan&& plan,
    core::conflict::ConflictManager&& conflicts,
    core::git::GitGraph<void>&& graph
) {

    auto& session = m_rebase.start(std::move(repo), std::move(plan), std::move(conflicts));
    m_controller  = std::make_unique<app::rebase::RebaseController>(session, m_conflict_editor, this);
    m_rebase_view->setSession(m_controller.get(), std::move(graph));

    auto* undo_stack = m_controller->undoStack();

    m_undo_group->addStack(undo_stack);
    m_undo_group->setActiveStack(undo_stack);

    m_layout->setCurrentIndex(page_rebase_view);
}

bool App::prepareRebaseSelection(core::git::RepoSession&& repo) {

    m_new_rebase_repo.emplace(std::move(repo));

    core::git::branch_iterator_t branch_iter;

    if (git_branch_iterator_new(&branch_iter, m_new_rebase_repo->repo(), GIT_BRANCH_LOCAL) != 0) {
        LOG_ERROR("branch iterator error; {}", core::git::get_last_error());
        return false;
    }

    git_branch_t branch_type;

    std::vector<std::string> branches;

    git_reference* branch_raw_ref;
    while (git_branch_next(&branch_raw_ref, &branch_type, branch_iter) == 0) {
        core::git::reference_t branch_ref { branch_raw_ref };

        const char* name;
        if (git_branch_name(&name, branch_ref) != 0) {
            continue;
        }

        branches.emplace_back(name);
    }

    m_rebase_select->setBranches(std::move(branches));

    return true;
}

void App::startNewRebase(QString commit_id) {
    auto branch_name = QString::fromStdString(m_rebase_select->selectedBranch());

    if (commit_id.isEmpty() || branch_name.isEmpty() || !m_new_rebase_repo.has_value()) {
        return;
    }

    git_oid oid;

    QByteArray bytes = commit_id.toLatin1();
    if (git_oid_fromstrn(&oid, bytes.constData(), bytes.size()) != 0) {
        return;
    }

    core::git::commit_t commit;
    if (git_commit_lookup(&commit, m_new_rebase_repo->repo(), &oid) != 0) {
        return;
    }

    const bool is_root_commit = git_commit_parentcount(commit) == 0;

    QString target;
    if (is_root_commit) {
        target = "--root";
    } else {
        // include parent parent so that the commit is included in todo list
        target = commit_id + "^";
    }

    QString git_path = QStandardPaths::findExecutable("git");
    if (git_path.isEmpty()) {
        DISPLAY_ERROR(this, "Rebase error", "Could not find git command.");
        return;
    }

    QString app_path = QApplication::applicationFilePath();
    if (logging::Log::is_debug()) {
        app_path += " --debug";
    }

    if (logging::Log::is_verbose()) {
        app_path += " --verbose";
    }

    app_path += " --edit-todo";

    QString cmd_str
        = QStringLiteral("%1 rebase --no-rebase-merges -i %2 %3").arg(git_path).arg(target).arg(branch_name);

    LOG_INFO(
        R"(Executing git.
    Command: {}
    WorkingDir: {}
    Editor: {})",
        cmd_str.toStdString(),
        m_new_rebase_repo->path(),
        app_path.toStdString()
    );

    auto answer = QMessageBox::question(
        this,
        "Execute command",
        QStringLiteral("Do you want to execute this command?\n- Command: %1\n- Working dir: %2")
            .arg(cmd_str)
            .arg(m_new_rebase_repo->path()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (answer == QMessageBox::No) {
        return;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GIT_SEQUENCE_EDITOR", app_path);

    m_git_proc = std::make_unique<QProcess>();
    m_git_proc->setProgram(git_path);
    m_git_proc->setArguments({ "rebase", "--no-rebase-merges", "-i", target, branch_name });
    m_git_proc->setWorkingDirectory(QString::fromStdString(m_new_rebase_repo->path()));
    m_git_proc->setProcessEnvironment(env);

    m_git_proc->setProcessChannelMode(QProcess::ForwardedChannels);
    m_git_proc->setInputChannelMode(QProcess::ForwardedInputChannel);

    connect(m_git_proc.get(), &QProcess::finished, this, [](int exitCode, QProcess::ExitStatus) {
        LOG_INFO("Git finished with exit code {}", exitCode);
        QApplication::exit(exitCode);
    });

    m_git_proc->start();

    if (!m_git_proc->waitForStarted()) {
        DISPLAY_ERROR(this, "Failed to execute git command", m_git_proc->errorString().toStdString());

        m_git_proc = nullptr;
        return;
    }

    setEnabled(false);
    hide();
}

void App::updateRebaseSelection(const std::string& branch) {
    if (branch.empty() || !m_new_rebase_repo.has_value()) {
        return;
    }

    m_rebase_select->clearCommits();
    m_rebase_select->clearMessage();

    core::git::branch_state_t state = core::git::check_branch(m_new_rebase_repo->repo(), branch.c_str());
    if (state.behind > 0) {
        m_rebase_select->setMessage(
            QStringLiteral("Warning: branch '%1' is %2 commit(s) behind upstream").arg(branch).arg(state.behind)
        );
    }

    core::git::iterate_branch_commits(m_new_rebase_repo->repo(), branch.c_str(), [this](git_commit* commit) {
        const auto* id     = git_commit_id(commit);
        std::string id_str = git_oid_tostr_s(id);

        const char* summary = git_commit_summary(commit);
        if (summary == nullptr) {
            return;
        }

        std::string name = std::format("[{}]: {}", core::git::format_oid_to_str(id), summary);

        m_rebase_select->addCommit(name, id_str);
    });
}

bool App::saveSaveFile(bool choose_file) {
    if (m_rebase.current() == nullptr) {
        return false;
    }

    if (!m_save_file.has_value() || choose_file) {
        QString filter = "XML Files (*.xml)";

        QFileDialog dialog(this);
        dialog.setWindowTitle("Save");
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setNameFilter("XML Files (*.xml)");
        dialog.setDirectory(QString::fromStdString(m_rebase.current()->repo().path()));

        dialog.setDefaultSuffix("xml");

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        QString filepath = dialog.selectedFiles().first();

        if (filepath.isEmpty()) {
            return false;
        }

        m_save_file = filepath;
    }

    LOG_INFO("Saving: {}", m_save_file->toStdString());

    if (!state::State::save(m_save_file.value().toStdU32String(), *m_rebase.current())) {
        DISPLAY_ERROR(this, "Save error", "Failed to save");
        return false;
    }

    m_controller->markSaved();
    return true;
}

void App::loadSaveFile() {
    QString repo_path;
    if (m_rebase.current() != nullptr) {
        repo_path = QString::fromStdString(m_rebase.current()->repo().path());
    }

    QString filter = "XML Files (*.xml)";
    QString dir    = m_save_file.value_or(repo_path);

    QString filepath = QFileDialog::getOpenFileName(this, "Load", dir, filter, nullptr);
    if (filepath.isEmpty()) {
        return;
    }

    LOG_INFO("Loading: {}", filepath.toStdString());

    auto save_data = state::State::load(filepath.toStdU32String());
    if (!save_data.has_value()) {
        auto err = save_data.error();
        DISPLAY_ERROR(this, err.title, err.message);
        return;
    }
    m_save_file = filepath;

    auto plan = core::rebase::RebasePlan::open(save_data->info, std::move(save_data->actions));

    openRebase(
        std::move(save_data->repo), std::move(plan), std::move(save_data->conflicts), std::move(save_data->graph)
    );
}

bool App::saveTodoFile(bool insert_break) {
    if (m_rebase.current() == nullptr) {
        return false;
    }

    const auto* rebase = m_rebase.current();
    const auto& repo   = rebase->repo();
    const auto& plan   = rebase->plan();
    const auto& info   = plan.info();

    const std::string& expected_head = info.head;
    const std::string& expected_onto = info.onto;

    auto tmp_info = repo.rebase_info();
    if (!tmp_info.has_value()) {
        auto err = tmp_info.error();
        DISPLAY_ERROR(this, err.title, err.message);
        return false;
    } else if (!tmp_info->has_value()) {
        DISPLAY_ERROR(
            this,
            "Rebase todo file error",
            "Cannot save todo file: Git rebase files not found.\nMake sure you're in the middle of an active rebase "
            "operation."
        );
        return false;
    }
    auto new_info = tmp_info->value();

    const auto& head = new_info.head;
    const auto& onto = new_info.onto;

    if (head != expected_head || onto != expected_onto) {
        DISPLAY_ERROR(
            this,
            "Rebase state mismatch",
            std::format(
                "Cannot save: The current rebase no longer matches what this application is editing.\n\n"
                "Expected - HEAD: {}, onto: {}\n"
                "Current  - HEAD: {}, onto: {}\n\n"
                "Git operations were performed that changed the rebase state.",
                expected_head.substr(0, 8),
                expected_onto.substr(0, 8),
                head.substr(0, 8),
                onto.substr(0, 8)
            )
        );

        return false;
    }

    auto filepath = repo.path() + '/' + core::git::TODO_FILE.c_str();
    std::ofstream todo_file(filepath);
    if (!todo_file.good()) {
        DISPLAY_ERROR(
            this, "File access error", "Cannot save rebase instructions. Unable to write to the git rebase todo file."
        );
        return false;
    }

    LOG_INFO("Saving todo file: {}", filepath);

    bool status = core::action::Converter::actions_to_todo(
        todo_file, *m_rebase.current(), *m_rebase_view->rebaseAnalysis(), insert_break
    );

    if (!status) {
        DISPLAY_ERROR(this, "Save error", "Failed to save rebase instructions");
        return false;
    }

    return true;
}
