#pragma once

#include "gui/widget/RebaseViewWidget.h"

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

class MainWindow : public QMainWindow {
public:
    MainWindow();
    bool openRepoDialog();
    bool openRepo(const std::string& path);

    ~MainWindow() override {
        git_repository_free(m_repo);
        git_libgit2_shutdown();
    }

private:
    std::string m_repo_path;
    QHBoxLayout* m_layout;
    RebaseViewWidget* m_rebase_view;
    git_repository* m_repo = nullptr;

    bool showRebase();
    void hideOldCommits(bool state);
    void openRepoCLI(const std::string& todo_file);
};
