#include "App.h"

#include "parser.h"

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

    // TOP BAR ------------------------------------------------------------
    auto* menu = new QMenuBar(this);
    setMenuBar(menu);

    auto* repo = menu->addMenu("Repo");

    auto* repo_open = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::FolderOpen), "Open", this);
    repo_open->setStatusTip("Open a repo");
    connect(repo_open, &QAction::triggered, this, &MainWindow::open_repo);

    repo->addAction(repo_open);

    // MAIN ---------------------------------------------------------------
    auto* main = new QWidget(this);
    setCentralWidget(main);

    m_layout = new QHBoxLayout(main);

    m_help_label = new QLabel("Open a repo with rebase in progress", main);
    m_layout->addWidget(m_help_label);

    m_rebase_view = new RebaseViewWidget();
    m_layout->addWidget(m_rebase_view);

    m_rebase_view->hide();
}

void MainWindow::open_repo() {
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

    show_rebase();
}

void MainWindow::show_rebase() {
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
