#pragma once

#include "ListItem.h"
#include "parser.h"
#include "RebaseViewWidget.h"

#include <cassert>
#include <cctype>
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

constexpr auto* TODO_FILE_PATH = ".git/rebase-merge/git-rebase-todo";
constexpr auto* HEAD_FILE      = ".git/rebase-merge/orig-head";
constexpr auto* ONTO_FILE      = ".git/rebase-merge/onto";

class MainWindow : public QMainWindow {
public:
    MainWindow();
    void open_repo();

    ~MainWindow() override {
        git_repository_free(m_repo);
        git_libgit2_shutdown();
    }

private:
    std::string m_repo_path;
    QWidget* m_help_label;
    QHBoxLayout* m_layout;
    RebaseViewWidget* m_rebase_view;
    git_repository* m_repo = nullptr;

    void show_rebase();
};
