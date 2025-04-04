#include "App.h"

#include "core/git/paths.h"
#include "core/state/CommandHistory.h"
#include "gui/widget/RebaseViewWidget.h"

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <fstream>
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
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QString>
#include <string>

MainWindow::MainWindow() {
    using core::state::CommandHistory;

    git_libgit2_init();

    QPalette palette;
    palette.setColor(QPalette::Window, Qt::white);
    setPalette(palette);
    // TOP BAR ------------------------------------------------------------
    auto* menu = new QMenuBar(this);
    setMenuBar(menu);

    auto* repo = menu->addMenu("Repo");

    auto* repo_open = new QAction(QIcon::fromTheme("folder-open"), "Open", this);
    repo_open->setStatusTip("Open a repo");

    connect(repo_open, &QAction::triggered, this, [this] {
        if (CommandHistory::CanUndo() || CommandHistory::CanRedo()) {
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

    repo->addAction(repo_open);

    {
        auto* edit = menu->addMenu("Edit");

        auto* edit_undo = new QAction(QIcon::fromTheme("edit-undo"), "Undo", this);
        edit_undo->setEnabled(false);
        connect(edit_undo, &QAction::triggered, this, [] { CommandHistory::Undo(); });

        auto* edit_redo = new QAction(QIcon::fromTheme("edit-redo"), "Redo", this);
        edit_redo->setEnabled(false);
        connect(edit_redo, &QAction::triggered, this, [] { CommandHistory::Redo(); });

        edit->addAction(edit_undo);
        edit->addAction(edit_redo);

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
        connect(hide_old_commits, &QAction::triggered, this, &MainWindow::hideOldCommits);

        auto* hide_result_commits = new QAction("Show result commits", this);

        hide_result_commits->setCheckable(true);
        hide_result_commits->setChecked(true);
        connect(hide_result_commits, &QAction::triggered, this, &MainWindow::hideResultCommits);

        view->addAction(hide_old_commits);
        view->addAction(hide_result_commits);
    }

    // MAIN ---------------------------------------------------------------
    auto* main = new QWidget(this);
    main->setContentsMargins(0, 0, 0, 0);
    setCentralWidget(main);

    m_layout = new QHBoxLayout(main);

    m_rebase_view = new RebaseViewWidget();
    m_layout->addWidget(m_rebase_view);

    m_rebase_view->hide();
    m_rebase_view->hideOldCommits();

    auto args = QApplication::arguments();
    args.removeFirst(); // program path

    if (!args.empty()) {
        // NOTE: Disable open repo action when opening from command line
        repo_open->setEnabled(false);
        openRepoCLI(args.front().toStdString());
    }
}

void MainWindow::hideOldCommits(bool state) {
    if (!state) {
        m_rebase_view->hideOldCommits();
    } else {
        m_rebase_view->showOldCommits();
    }
}

void MainWindow::hideResultCommits(bool state) {
    if (!state) {
        m_rebase_view->hideResultCommits();
    } else {
        m_rebase_view->showResultCommits();
    }
}

bool MainWindow::openRepoDialog() {
    auto folder = QFileDialog::getExistingDirectory(
        this, "Select Repo folder", QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (folder.isEmpty()) {
        return false;
    }

    auto path = folder.toStdString();
    return openRepo(path);
}

void MainWindow::openRepoCLI(const std::string& path) {

    std::string repo_path = path;
    if (!std::filesystem::is_directory(path)) {
        repo_path = core::git::repo_path_from_todo(path);
    }

    if (!openRepo(repo_path)) {
        // NOTE: If this method is called before QApplication::exec() then it will do nothing.
        QApplication::exit(1);

        // NOTE: Only called in before QApplication::exec()
        std::exit(1);
    }
}

bool MainWindow::openRepo(const std::string& path) {
    m_rebase_view->hide();
    core::state::CommandHistory::Clear();

    if (git_repository_open(&m_repo, path.c_str()) != 0) {
        git_repository_free(m_repo);
        m_repo = nullptr;

        const auto* err = git_error_last();
        QMessageBox::critical(this, "Repo Error", err->message);
        return false;
    }

    m_repo_path = path;

    return showRebase();
}

bool MainWindow::showRebase() {
    std::string head;
    std::string onto;

    {
        auto head_file = std::ifstream(m_repo_path + '/' + core::git::HEAD_FILE.c_str());
        auto onto_file = std::ifstream(m_repo_path + '/' + core::git::ONTO_FILE.c_str());

        if (!head_file.good() || !onto_file.good()) {
            QMessageBox::critical(this, "Rebase Error", "Could not find rebase files");
            return false;
        }

        std::getline(head_file, head);
        std::getline(onto_file, onto);
    }

    auto filepath = m_repo_path + '/' + core::git::TODO_FILE.c_str();

    auto res = parse_file(filepath);
    if (!res.err.empty()) {
        QMessageBox::critical(this, "Rebase Error", res.err.c_str());
        return false;
    }

    auto rebase_res = m_rebase_view->update(m_repo, head, onto, res.actions);

    if (rebase_res.has_value()) {
        QMessageBox::critical(this, "Rebase Error", rebase_res.value().c_str());
        return false;
    }

    m_rebase_view->show();

    return true;
}
