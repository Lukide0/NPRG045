#pragma once

#include "gui/widget/RebaseViewWidget.h"
#include "gui/window/PreferencesWindow.h"

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

    void openRepoCLI(const std::string& todo_file);

    static void updateGraph();
    static void updateActions();
    static gui::widget::RebaseViewWidget* getRebaseViewWidget();
    static const std::string& getRepoPath();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    enum class SaveStatus {
        SAVE,
        DISCARD,
        CANCEL,
    };

    bool m_cli_start = false;
    QAction* m_repo_open;

    std::string m_repo_path;
    std::string m_rebase_head;
    std::string m_rebase_onto;

    QHBoxLayout* m_layout;
    gui::widget::RebaseViewWidget* m_rebase_view;
    gui::window::PreferencesWindow* m_preferences;

    git_repository* m_repo = nullptr;

    std::optional<QString> m_save_file;

    bool showRebase();
    void hideOldCommits(bool state);
    void hideResultCommits(bool state);

    void loadSaveFile();
    bool saveTodoFile();
    bool saveSaveFile(bool choose_file);

    void showPreferences();

    SaveStatus maybeSave();
};
