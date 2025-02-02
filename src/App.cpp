#include "App.h"

#include "git/parser.h"

#include <cassert>
#include <cctype>
#include <fstream>
#include <git2.h>
#include <git2/commit.h>
#include <git2/errors.h>
#include <git2/global.h>
#include <git2/repository.h>
#include <git2/types.h>
#include <QAction>
#include <QFileDialog>
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
    git_libgit2_init();

    QPalette palette;
    palette.setColor(QPalette::Window, Qt::white);
    setPalette(palette);
    // TOP BAR ------------------------------------------------------------
    auto* menu = new QMenuBar(this);
    setMenuBar(menu);

    auto* repo = menu->addMenu("Repo");

    auto* repo_open = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::FolderOpen), "Open", this);
    repo_open->setStatusTip("Open a repo");
    connect(repo_open, &QAction::triggered, this, &MainWindow::openRepo);

    auto* repo_refresh = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::ViewRefresh), "Refresh", this);
    connect(repo_refresh, &QAction::triggered, this, [&]() { showRebase(); });

    repo->addAction(repo_open);
    repo->addAction(repo_refresh);

    auto* view             = menu->addMenu("View");
    auto* hide_old_commits = new QAction("Hide old commits", this);

    hide_old_commits->setCheckable(true);
    hide_old_commits->setChecked(true);
    connect(hide_old_commits, &QAction::triggered, this, &MainWindow::hideOldCommits);

    view->addAction(hide_old_commits);

    // MAIN ---------------------------------------------------------------
    auto* main = new QWidget(this);
    setCentralWidget(main);

    m_layout = new QHBoxLayout(main);

    m_help_label = new QLabel("Open a repo with rebase in progress", main);
    m_layout->addWidget(m_help_label);

    m_rebase_view = new RebaseViewWidget();
    m_layout->addWidget(m_rebase_view);

    m_rebase_view->hide();
    m_rebase_view->hideOldCommits();
}

void MainWindow::hideOldCommits(bool state) {
    if (state) {
        m_rebase_view->hideOldCommits();
    } else {
        m_rebase_view->showOldCommits();
    }
}

void MainWindow::openRepo() {
    auto folder = QFileDialog::getExistingDirectory(
        this, "Select Repo folder", QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (folder.isEmpty()) {
        return;
    }

    auto path = folder.toStdString();

    m_rebase_view->hide();

    if (git_repository_open(&m_repo, path.c_str()) != 0) {

        m_help_label->show();

        git_repository_free(m_repo);
        m_repo = nullptr;

        const auto* err = git_error_last();
        QMessageBox::critical(this, "Repo Error", err->message);
        return;
    }

    m_repo_path = path;

    showRebase();
}

void MainWindow::showRebase() {
    std::string head;
    std::string onto;

    {
        auto head_file = std::ifstream(m_repo_path + '/' + HEAD_FILE);
        auto onto_file = std::ifstream(m_repo_path + '/' + ONTO_FILE);

        if (!head_file.good() || !onto_file.good()) {
            QMessageBox::critical(this, "Rebase Error", "Could not find rebase files");
            return;
        }

        std::getline(head_file, head);
        std::getline(onto_file, onto);
    }

    auto filepath = m_repo_path + '/' + TODO_FILE_PATH;

    auto res = parse_file(filepath);
    if (!res.err.empty()) {
        QMessageBox::critical(this, "Rebase Error", res.err.c_str());
        return;
    }

    auto rebase_res = m_rebase_view->update(m_repo, head, onto, res.actions);

    if (rebase_res.has_value()) {
        QMessageBox::critical(this, "Rebase Error", rebase_res.value().c_str());
        return;
    }

    m_help_label->hide();
    m_rebase_view->show();
}
