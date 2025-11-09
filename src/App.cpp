#include "App.h"

#include "action/Converter.h"
#include "core/git/parser.h"
#include "core/git/paths.h"
#include "core/state/CommandHistory.h"
#include "core/state/State.h"
#include "core/utils/optional_uint.h"
#include "gui/widget/RebaseViewWidget.h"
#include "gui/widget/SettingsDialog.h"
#include "logging/Log.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <git2.h>
#include <git2/commit.h>
#include <git2/errors.h>
#include <git2/global.h>
#include <git2/repository.h>
#include <git2/types.h>

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <qnamespace.h>
#include <QPalette>
#include <QString>
#include <utility>

static App* g_app = nullptr;

void App::updateGraph() { g_app->m_rebase_view->updateGraph(); }

void App::updateActions() { g_app->m_rebase_view->updateActions(); }

gui::widget::RebaseViewWidget* App::getRebaseViewWidget() { return g_app->m_rebase_view; }

const std::string& App::getRepoPath() { return g_app->m_repo_path; }

App::App() {

    g_app = this;

    using core::state::CommandHistory;

    git_libgit2_init();

    QPalette palette;
    palette.setColor(QPalette::Window, Qt::white);
    setPalette(palette);

    // TOP BAR ------------------------------------------------------------
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

        auto* edit_todo_save = new QAction(QIcon::fromTheme("document-save-as"), "Save Todo");
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

    m_layout = new QHBoxLayout(main);

    m_rebase_view    = new gui::widget::RebaseViewWidget();
    m_welcome_widget = new gui::widget::WelcomeWidget();

    m_layout->addWidget(m_rebase_view);
    m_layout->addWidget(m_welcome_widget);

    m_rebase_view->hide();
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

    connect(m_welcome_widget, &gui::widget::WelcomeWidget::loadSaveFile, this, [this]() {
        // NOTE: loadSaveFile handles widget visibility based on success/failure
        loadSaveFile();
    });
}

App::SaveStatus App::maybeSave() {
    using core::state::CommandHistory;

    SaveStatus status = SaveStatus::SAVE;

    if (!CommandHistory::IsSaved()) {
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

    if (m_cli_start) {
        auto ans = QMessageBox::warning(
            this,
            "Save Rebase Plan",
            "Do you want to save your changes to the rebase plan? This file tells git how to replay your commits "
            "during the rebase.",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (ans == QMessageBox::Save) {
            if (!saveTodoFile()) {
                status = SaveStatus::CANCEL;
            }
        } else if (ans == QMessageBox::Cancel) {
            status = SaveStatus::CANCEL;
        } else if (ans == QMessageBox::Discard) {
            status = SaveStatus::DISCARD;
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
    LOG_INFO("Openning repo '{}'", path);

    m_rebase_view->hide();
    core::state::CommandHistory::Clear();

    if (git_repository_open(&m_repo, path.c_str()) != 0) {
        git_repository_free(m_repo);
        m_repo = nullptr;

        const auto* err = git_error_last();
        QMessageBox::critical(this, "Repo Error", err->message);
        LOG_ERROR("Failed to open repo: {}", err->message);

        return false;
    }

    m_repo_path = path;

    return showRebase();
}

bool App::showRebase() {

    auto err = core::git::get_rebase_info(m_repo_path, m_rebase_head, m_rebase_onto);
    if (err.has_value()) {
        QMessageBox::critical(this, "Rebase Error", err.value());
        LOG_ERROR("{}", err.value());
        return false;
    }

    auto filepath = m_repo_path + '/' + core::git::TODO_FILE.c_str();

    auto res = core::git::parse_file(filepath);
    if (!res.err.empty()) {
        QMessageBox::critical(this, "Rebase Error", res.err.c_str());
        LOG_ERROR("Failed to parse todo file: {}", res.err);
        return false;
    }

    auto rebase_res = m_rebase_view->update(m_repo, m_rebase_head, m_rebase_onto, res.actions);

    if (rebase_res.has_value()) {
        QMessageBox::critical(this, "Rebase Error", rebase_res.value().c_str());
        return false;
    }

    m_rebase_view->show();
    m_welcome_widget->hide();

    return true;
}

bool App::saveSaveFile(bool choose_file) {
    if (!m_save_file.has_value() || choose_file) {
        QString default_path = QString::fromStdString(m_repo_path);

        QString filter = tr("XML Files (*.xml)");

        QFileDialog dialog(this);
        dialog.setWindowTitle("Save");
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setNameFilter("XML Files (*.xml)");
        dialog.setDirectory(QString::fromStdString(m_repo_path));

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

    if (!core::state::State::save(m_save_file.value().toStdU32String(), m_repo_path, m_rebase_head, m_rebase_onto)) {
        QMessageBox::critical(this, "Save error", "Failed to save");
        return false;
    }

    core::state::CommandHistory::Save();

    return true;
}

bool App::loadSaveFile() {
    QString filter = "XML Files (*.xml)";
    QString dir    = m_save_file.value_or(QString::fromStdString(m_repo_path));

    QString filepath = QFileDialog::getOpenFileName(this, "Load", dir, filter, nullptr);

    if (filepath.isEmpty()) {
        return false;
    }

    LOG_INFO("Loading: {}", filepath.toStdString());

    git_repository* repo;
    auto save_data = core::state::State::load(filepath.toStdU32String(), &repo);
    if (!save_data.has_value()) {
        QMessageBox::critical(this, "Load error", "Failed to load save file");
        return false;
    }

    if (m_repo != repo) {
        git_repository_free(m_repo);
    }

    m_rebase_view->hide();

    m_save_file = filepath;
    m_repo      = repo;

    m_rebase_head = save_data->head;
    m_rebase_onto = save_data->onto;

    auto& act_manager = action::ActionsManager::get();
    act_manager.clear();

    for (auto&& [act, msg] : save_data->actions) {
        if (!msg.empty()) {
            act.set_msg_id(optional_u31::some(act_manager.add_msg(msg)));
        }

        act_manager.append(std::move(act));
    }

    auto& conflict_manager = core::conflict::ConflictManager::get();
    conflict_manager.clear();

    for (auto&& [entry, blob] : save_data->conflicts) {
        conflict_manager.add_resolution(entry, blob);
    }

    for (auto&& [conflict, tree] : save_data->conflict_commits) {
        conflict_manager.add_commits_resolution(conflict, std::move(tree));
    }

    core::state::CommandHistory::Clear();

    m_rebase_view->show();
    m_welcome_widget->hide();

    auto rebase_res = m_rebase_view->update(m_repo, save_data->head, save_data->onto);
    if (rebase_res.has_value()) {
        QMessageBox::critical(this, "Rebase Error", rebase_res.value().c_str());
    }

    return true;
}

bool App::saveTodoFile() {
    std::string head;
    std::string onto;

    auto err = core::git::get_rebase_info(m_repo_path, head, onto);
    if (err.has_value()) {
        QMessageBox::critical(
            this,
            "Rebase Error",
            "Cannot save: Git rebase files not found.\n\n"
            "Make sure you're in the middle of an active rebase operation."
        );
        return false;
    }

    if (head != m_rebase_head || onto != m_rebase_onto) {
        QMessageBox::critical(
            this,
            "Rebase State Mismatch",
            QString(
                "Cannot save: The current rebase no longer matches what this application is editing.\n\n"
                "Expected - HEAD: %1, onto: %2\n"
                "Current  - HEAD: %3, onto: %4\n\n"
                "Git operations were performed that changed the rebase state."
            )
                .arg(QString::fromStdString(m_rebase_head.substr(0, 8)))
                .arg(QString::fromStdString(m_rebase_onto.substr(0, 8)))
                .arg(QString::fromStdString(head.substr(0, 8)))
                .arg(QString::fromStdString(onto.substr(0, 8)))
        );

        return false;
    }

    auto filepath = m_repo_path + '/' + core::git::TODO_FILE.c_str();

    std::ofstream todo_file(filepath);
    if (!todo_file.good()) {
        QMessageBox::critical(
            this,
            "File Access Error",
            "Cannot save rebase instructions.\n\n"
            "Unable to write to the git rebase file."
        );
        return false;
    }

    LOG_INFO("Saving todo file: {}", filepath);

    auto& manager = action::ActionsManager::get();
    bool status   = action::Converter::actions_to_todo(todo_file, manager, core::conflict::ConflictManager::get());

    if (!status) {
        QMessageBox::critical(this, "Save Error", "Failed to save rebase instructions.");
        return false;
    }

    return true;
}
