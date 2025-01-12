#include "parser.h"

#include <cassert>
#include <cctype>
#include <git2.h>
#include <git2/errors.h>
#include <git2/global.h>
#include <git2/repository.h>
#include <git2/types.h>
#include <iostream>
#include <QAction>
#include <QApplication>
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
#include <vector>

constexpr auto* TODO_FILE_PATH = ".git/rebase-merge/git-rebase-todo";

void clear_layout(QLayout* layout) {
    if (layout == nullptr) {
        return;
    }

    while (auto* child = layout->takeAt(0)) {
        if (auto* w = child->widget()) {
            w->deleteLater();
        } else if (auto* l = child->layout()) {
            clear_layout(l);
        }

        delete child;
    }
}

class MainWindow : public QMainWindow {
public:
    MainWindow() {
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
    }

    void open_repo() {
        auto folder = QFileDialog::getExistingDirectory(
            this, "Select Repo folder", QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

        if (folder.isEmpty()) {
            return;
        }

        auto path = folder.toStdString();

        if (git_repository_open(&m_repo, path.c_str()) != 0) {

            git_repository_free(m_repo);
            m_repo = nullptr;

            const auto* err = git_error_last();
            QMessageBox::critical(this, "Repo Error", err->message);
            return;
        }

        m_repo_path = path;

        show_rebase();
    }

    ~MainWindow() override {
        git_repository_free(m_repo);
        git_libgit2_shutdown();
    }

private:
    std::string m_repo_path;
    QWidget* m_help_label;
    QHBoxLayout* m_layout;
    git_repository* m_repo = nullptr;

    void show_rebase() {
        auto filepath = m_repo_path + '/' + TODO_FILE_PATH;

        auto res = parse_file(filepath);
        if (!res.err.empty()) {
            QMessageBox::critical(this, "Rebase Error", res.err.c_str());
            return;
        }

        display_commit_actions(res.actions);
    }

    void display_commit_actions(const std::vector<CommitAction>& actions) {
        clear_layout(m_layout);

        auto* list = new QListWidget();

        for (auto&& action : actions) {
            auto* item = new QListWidgetItem(list);
            std::cout << action.hash << std::endl;

            auto name = QString(cmd_to_str(action.type));
            name += " -- ";
            name += action.hash.c_str();

            item->setText(name);
            list->addItem(item);
        }

        m_layout->addWidget(list);
    }
};

int main(int argc, char* argv[]) {

    QApplication app(argc, argv);

    MainWindow main_window;
    main_window.show();

    return QApplication::exec();
}
