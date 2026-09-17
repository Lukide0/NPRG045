#include "App.h"

#include "action/Action.h"
#include "action/Converter.h"
#include "conflict/ConflictManager.h"
#include "git/commit.h"
#include "git/error.h"
#include "git/parser.h"
#include "git/paths.h"
#include "git/types.h"
#include "gui/error.h"
#include "gui/style/StyleManager.h"
#include "gui/widget/ListItem.h"
#include "gui/widget/RebaseSelectionWidget.h"
#include "gui/widget/RebaseViewWidget.h"
#include "gui/widget/SettingsDialog.h"
#include "gui/widget/WelcomeWidget.h"
#include "logging/Log.h"
#include "state/CommandHistory.h"
#include "state/State.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
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

void App::updateGraph() { g_app->m_rebase_view->updateGraph(); }

void App::updateActions() { g_app->m_rebase_view->updateActions(); }

void App::updateConflicts(action::Action* start) { g_app->m_rebase_view->updateConflicts(start); }

gui::widget::RebaseViewWidget* App::getRebaseViewWidget() { return g_app->m_rebase_view; }

const std::string& App::getRepoPath() { return g_app->m_state.repo_path(); }

QMap<QString, App::ShortcutAction>& App::getShortcuts() { return g_app->m_shortcuts; }

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

    git_libgit2_init();

    QSettings settings = App::getSettings();
    LOG_INFO("User settings: {}", settings.fileName().toStdString());

    // load styles
    {
        auto& manager = gui::style::StyleManager::get();
        manager.load_styles(settings);
    }

    QPalette palette;
    palette.setColor(QPalette::Window, Qt::white);
    setPalette(palette);

    setup();
    setupShortcuts();

    App::loadShortcuts(settings);
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
        QListWidget* actions_list = m_rebase_view->getList();
        auto* actions_move_up     = create_shortcut("actions.move_up", actions_list, "Move focused action up");
        auto* actions_move_down   = create_shortcut("actions.move_down", actions_list, "Move focused action down");
        auto* actions_change      = create_shortcut("actions.change_type", actions_list, "Change focused action type");

        constexpr auto action_types = gui::widget::ListItem::items;

        for (action::ActionType type : action_types) {
            const char* name = action::type_to_str(type);
            QString desc     = QString("Change focused action type to '%1'").arg(name);
            QString id       = QString("actions.change_%1").arg(name);

            auto* change_type = create_shortcut(id, actions_list, desc);

            connect(change_type, &QAction::triggered, this, [this, type]() { m_rebase_view->changeActionType(type); });
        }

        auto* actions_change_pick
            = create_shortcut("actions.change_pick", actions_list, "Change focused action type to 'pick'");

        connect(actions_move_up, &QAction::triggered, this, [this]() { m_rebase_view->moveActionUp(); });
        connect(actions_move_down, &QAction::triggered, this, [this]() { m_rebase_view->moveActionDown(); });
        connect(actions_change, &QAction::triggered, this, [this]() { m_rebase_view->changeActionType(); });

        connect(actions_change_pick, &QAction::triggered, this, [this]() {
            m_rebase_view->changeActionType(action::ActionType::PICK);
        });
    }
}

void App::setup() {
    using state::CommandHistory;

    auto* menu = new QMenuBar(this);
    setMenuBar(menu);

    auto* repo  = menu->addMenu("Repo");
    m_repo_open = new QAction(QIcon::fromTheme("folder-open"), "Open", this);

    {
        m_repo_open->setStatusTip("Open a repo");

        connect(m_repo_open, &QAction::triggered, this, [this] {
            if (CommandHistory::CanUndo()) {
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

        auto* edit_undo = new QAction(QIcon::fromTheme("edit-undo"), "Undo", this);
        edit_undo->setEnabled(false);
        connect(edit_undo, &QAction::triggered, this, [] { CommandHistory::Undo(); });

        auto* edit_redo = new QAction(QIcon::fromTheme("edit-redo"), "Redo", this);
        edit_redo->setEnabled(false);
        connect(edit_redo, &QAction::triggered, this, [] { CommandHistory::Redo(); });

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
            gui::widget::SettingsDialog dialog(this);
            dialog.exec();
        });

        edit->addAction(edit_undo);
        edit->addAction(edit_redo);
        edit->addSeparator();
        edit->addAction(edit_save);
        edit->addAction(edit_save_as);
        edit->addAction(m_load_save);
        edit->addAction(edit_todo_save);
        edit->addSeparator();
        edit->addAction(edit_preferences);

        edit_undo->setShortcut(QKeySequence::Undo);
        edit_redo->setShortcut(QKeySequence::Redo);

        CommandHistory::SetUndo(edit_undo);
        CommandHistory::SetRedo(edit_redo);
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

    m_rebase_view    = new gui::widget::RebaseViewWidget();
    m_welcome_widget = new gui::widget::WelcomeWidget();
    m_rebase_select  = new gui::widget::RebaseSelectionWidget();

    auto* welcome_page   = new QWidget();
    auto* welcome_layout = new QHBoxLayout();
    welcome_page->setLayout(welcome_layout);

    welcome_layout->addWidget(m_welcome_widget);

    m_layout->addWidget(welcome_page);
    m_layout->addWidget(m_rebase_view);
    m_layout->addWidget(m_rebase_select);

    m_rebase_view->hideOldCommits();

    connect(m_welcome_widget, &gui::widget::WelcomeWidget::openCurrentDirectory, this, [this]() {
        QString currentPath = QDir::currentPath();

        // NOTE: openRepo handles widget visibility based on success/failure
        openRepo(currentPath.toStdString());
    });

    connect(m_welcome_widget, &gui::widget::WelcomeWidget::openRepository, this, [this]() {
        // NOTE: openRepoDialog handles widget visibility based on success/failure
        openRepoDialog();
    });

    connect(m_welcome_widget, &gui::widget::WelcomeWidget::loadSaveFile, this, &App::loadSaveFile);

    connect(
        m_rebase_select, &gui::widget::RebaseSelectionWidget::branchSelectionChanged, this, &App::updateRebaseSelection
    );

    connect(m_rebase_select, &gui::widget::RebaseSelectionWidget::cancelled, this, [this]() {
        m_layout->setCurrentIndex(page_welcome);
    });

    connect(m_rebase_select, &gui::widget::RebaseSelectionWidget::completed, this, &App::startNewRebase);
}

App::SaveStatus App::maybeSave() {
    using state::CommandHistory;

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
    } else if (!CommandHistory::IsSaved()) {
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
        repo_path = git::repo_path_from_todo(path);
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

    state::CommandHistory::Clear();

    git::repository_t new_repo;
    if (git_repository_open(&new_repo, path.c_str()) != 0) {

        DISPLAY_LIBGIT_ERROR(this, "Failed to open repository", git::get_last_error());

        m_layout->setCurrentIndex(page_welcome);
        return false;
    }

    m_state.set_repo(std::move(new_repo), path);

    std::string head, onto;
    auto err = git::get_rebase_info(path, head, onto);

    m_state.set_rebase(head, onto);

    if (err.has_value()) {

        if (err->type == git::RebaseInfoError::NotFound) {
            m_layout->setCurrentIndex(page_rebase_select);

            if (!prepareRebaseSelection()) {
                m_layout->setCurrentIndex(page_welcome);
                return false;
            } else {
                return true;
            }
        }

        DISPLAY_ERROR(this, "Rebase error", err->msg);
        m_layout->setCurrentIndex(page_welcome);
        return false;
    }

    if (!loadRebase()) {
        m_layout->setCurrentIndex(page_welcome);
        return false;
    }

    m_layout->setCurrentIndex(page_rebase_view);
    return true;
}

bool App::loadRebase() {
    auto filepath = m_state.repo_path() + '/' + git::TODO_FILE.c_str();

    auto res = git::parse_file(filepath);
    if (!res.err.empty()) {
        DISPLAY_ERROR(this, "Failed to parse todo file", res.err);
        return false;
    }

    auto rebase_res = m_rebase_view->update(m_state, res.actions);
    if (rebase_res.has_value()) {
        DISPLAY_ERROR(this, "Rebase error", *rebase_res);
        return false;
    }

    return true;
}

bool App::prepareRebaseSelection() {

    git::branch_iterator_t branch_iter;

    if (git_branch_iterator_new(&branch_iter, m_state.repo(), GIT_BRANCH_LOCAL) != 0) {
        return false;
    }

    git::reference_t branch_ref;
    git_branch_t branch_type;

    std::vector<std::string> branches;

    while (git_branch_next(&branch_ref, &branch_type, branch_iter) == 0) {
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

    if (commit_id.isEmpty() || branch_name.isEmpty()) {
        return;
    }

    git_oid oid;

    QByteArray bytes = commit_id.toLatin1();
    if (git_oid_fromstrn(&oid, bytes.constData(), bytes.size()) != 0) {
        return;
    }

    git::commit_t commit;
    if (git_commit_lookup(&commit, m_state.repo(), &oid) != 0) {
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

    QString cmd_str = QString("%1 rebase --no-rebase-merges -i %2 %3").arg(git_path).arg(target).arg(branch_name);

    LOG_INFO(
        R"(Executing git.
    Command: {}
    WorkingDir: {}
    Editor: {})",
        cmd_str.toStdString(),
        m_state.repo_path(),
        app_path.toStdString()
    );

    auto answer = QMessageBox::question(
        this,
        "Execute command",
        QString("Do you want to execute this command?\n- Command: %1\n- Working dir: %2")
            .arg(cmd_str)
            .arg(m_state.repo_path()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (answer == QMessageBox::No) {
        return;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GIT_SEQUENCE_EDITOR", app_path);

    QProcess git_proc;
    git_proc.setProgram(git_path);
    git_proc.setArguments({ "rebase", "--no-rebase-merges", "-i", target, branch_name });
    git_proc.setWorkingDirectory(QString::fromStdString(m_state.repo_path()));
    git_proc.setProcessEnvironment(env);

    qint64 pid;
    if (!git_proc.startDetached(&pid)) {
        DISPLAY_ERROR(this, "Failed to execute git command", git_proc.errorString().toStdString());
        return;
    }

    // close this application and let git reopen it with the todo file
    QApplication::quit();
}

void App::updateRebaseSelection(const std::string& branch) {
    if (branch.empty() || m_state.repo() == nullptr) {
        return;
    }

    m_rebase_select->clearCommits();
    m_rebase_select->clearMessage();

    git::branch_state_t state = git::check_branch(m_state.repo(), branch.c_str());
    if (state.behind > 0) {
        m_rebase_select->setMessage(
            QString("Warning: branch '%1' is %2 commit(s) behind upstream").arg(branch).arg(state.behind)
        );
    }

    git::iterate_branch_commits(m_state.repo(), branch.c_str(), [this](git_commit* commit) {
        const auto* id     = git_commit_id(commit);
        std::string id_str = git_oid_tostr_s(id);

        const char* summary = git_commit_summary(commit);
        if (summary == nullptr) {
            return;
        }

        std::string name = std::format("[{}]: {}", git::format_oid_to_str(id), summary);

        m_rebase_select->addCommit(name, id_str);
    });
}

bool App::saveSaveFile(bool choose_file) {
    if (!m_save_file.has_value() || choose_file) {
        QString default_path = QString::fromStdString(m_state.repo_path());

        QString filter = tr("XML Files (*.xml)");

        QFileDialog dialog(this);
        dialog.setWindowTitle("Save");
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setNameFilter("XML Files (*.xml)");
        dialog.setDirectory(QString::fromStdString(m_state.repo_path()));

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

    if (!state::State::save(m_save_file.value().toStdU32String(), m_state)) {
        DISPLAY_ERROR(this, "Save error", "Failed to save");
        return false;
    }

    state::CommandHistory::Save();

    return true;
}

void App::loadSaveFile() {
    QString filter = "XML Files (*.xml)";
    QString dir    = m_save_file.value_or(QString::fromStdString(m_state.repo_path()));

    QString filepath = QFileDialog::getOpenFileName(this, "Load", dir, filter, nullptr);

    if (filepath.isEmpty()) {
        return;
    }

    LOG_INFO("Loading: {}", filepath.toStdString());

    git::repository_t repo;
    auto save_data = state::State::load(filepath.toStdU32String(), &repo);
    if (!save_data.has_value()) {
        DISPLAY_ERROR(this, "Load save error", "Failed to load save file");
        return;
    }

    m_save_file = filepath;

    m_state.set_repo(std::move(repo), save_data->repo_path);
    m_state.set_rebase(save_data->head, save_data->onto);

    auto& act_manager = action::ActionsManager::get();
    act_manager.clear();

    for (auto&& [act, msg] : save_data->actions) {
        if (!msg.empty()) {
            act.set_msg_id(optional_u31::some(act_manager.add_msg(msg)));
        }

        act_manager.append(std::move(act));
    }

    auto& conflict_manager = conflict::ConflictManager::get();
    conflict_manager.clear();

    for (auto&& [entry, blob] : save_data->conflicts) {
        conflict_manager.add_resolution(entry, blob);
    }

    for (auto&& [conflict, tree] : save_data->conflict_trees) {
        conflict_manager.add_trees_resolution(conflict, std::move(tree));
    }

    state::CommandHistory::Clear();

    m_layout->setCurrentIndex(page_rebase_view);

    auto rebase_res = m_rebase_view->update(m_state);
    if (rebase_res.has_value()) {
        DISPLAY_ERROR(this, "Rebase error", *rebase_res);
    }
}

bool App::saveTodoFile(bool insert_break) {
    std::string head;
    std::string onto;

    const std::string& expected_head = m_state.rebase_head();
    const std::string& expected_onto = m_state.rebase_onto();

    auto err = git::get_rebase_info(m_state.repo_path(), head, onto);
    if (err.has_value()) {
        DISPLAY_ERROR(
            this,
            "Rebase todo file error",
            "Cannot save todo file: Git rebase files not found.\nMake sure you're in the middle of an active rebase "
            "operation."
        );
        return false;
    }

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

    auto filepath = m_state.repo_path() + '/' + git::TODO_FILE.c_str();

    std::ofstream todo_file(filepath);
    if (!todo_file.good()) {
        DISPLAY_ERROR(
            this, "File access error", "Cannot save rebase instructions. Unable to write to the git rebase todo file."
        );
        return false;
    }

    LOG_INFO("Saving todo file: {}", filepath);

    auto& manager = action::ActionsManager::get();
    bool status
        = action::Converter::actions_to_todo(todo_file, manager, conflict::ConflictManager::get(), insert_break);

    if (!status) {
        DISPLAY_ERROR(this, "Save error", "Failed to save rebase instructions");
        return false;
    }

    return true;
}
