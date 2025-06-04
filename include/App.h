#pragma once

#include "gui/widget/RebaseViewWidget.h"

#include <cassert>
#include <cctype>
#include <string>

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

class App : public QMainWindow {
public:
    App();
    bool openRepoDialog();
    bool openRepo(const std::string& path);

    ~App() override {
        git_repository_free(m_repo);
        git_libgit2_shutdown();
    }

    static void updateGraph();
    static void updateActions();
    static gui::widget::RebaseViewWidget* getRebaseViewWidget();

private:
    std::string m_repo_path;
    QHBoxLayout* m_layout;
    gui::widget::RebaseViewWidget* m_rebase_view;
    git_repository* m_repo = nullptr;

    bool showRebase();
    void hideOldCommits(bool state);
    void hideResultCommits(bool state);
    void openRepoCLI(const std::string& todo_file);
};
