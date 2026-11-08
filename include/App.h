#pragma once

#include "app/conflict/ConflictEditor.h"
#include "app/Settings.h"
#include "core/rebase/RebaseManager.h"
#include "gui/rebase/RebaseSelectionWidget.h"
#include "gui/rebase/RebaseViewWidget.h"
#include "gui/widget/WelcomeWidget.h"

#include <cassert>
#include <memory>
#include <optional>
#include <string>

#include <git2.h>
#include <git2/commit.h>
#include <git2/errors.h>
#include <git2/global.h>
#include <git2/repository.h>
#include <git2/types.h>

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMainWindow>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QStackedLayout>
#include <QString>
#include <QUndoGroup>

/**
 * @brief Main application window
 */
class App : public QMainWindow {
public:
    /**
     * @brief Shortcut action
     */
    struct ShortcutAction {
        QAction* action;
        QString description;
        QKeySequence default_shortcut;
    };

    App();

    /**
     * @brief Opens a repository selection dialog.
     * @return True if a repository was successfully opened, false otherwise.
     */
    bool openRepoDialog();

    /**
     * @brief Opens a repository.
     * @param path Path to directory containing git repository (".git")
     * @return True if a repository was successfully opened, false otherwise.
     */
    bool openRepo(const std::string& path);

    ~App() override = default;

    /**
     * @brief Opens a repository and disables loading or opening other repositories.
     *
     * @param todo_file Path to the Git rebase todo file.
     *
     * @details If the repository cannot be opened successfully, the application
     * exits with code 1.
     */
    void openRepoCLI(const std::string& todo_file);

    /**
     * @brief Returns application settings.
     */
    static QSettings getSettings();

    /**
     * @brief Returns conflict editor.
     */
    static app::conflict::ConflictEditor& getConflictEditor();

    /**
     * @brief Returns shortcuts.
     */
    static QMap<QString, ShortcutAction>& getShortcuts();

    /**
     * @brief Registers a shortcut.
     */
    static void registerShortcut(const QString& action_id, QAction* action, const QString& description);

    /**
     * @brief Loads shortcuts from settings.
     */
    static void loadShortcuts(QSettings& settings);

    /**
     * @brief Saves shortcuts to settings.
     */
    static void saveShortcuts(QSettings& settings);

    static const core::rebase::RebaseSession* rebaseSession();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    /**
     * @brief Represents the save dialog result.
     */
    enum class SaveStatus {
        SAVE, /**< Save changes */
        DISCARD, /**< Discard changes */
        CANCEL, /**< Cancel action */
    };

    static constexpr int page_welcome       = 0;
    static constexpr int page_rebase_view   = 1;
    static constexpr int page_rebase_select = 2;

    /**
     * @brief Indicates whether the application was started with a path argument.
     */
    bool m_cli_start = false;

    core::rebase::RebaseManager m_rebase;
    std::unique_ptr<app::rebase::RebaseController> m_controller;
    QUndoGroup* m_undo_group;

    QAction* m_repo_open;
    QAction* m_load_save;

    struct {
        QAction* undo;
        QAction* redo;
    } m_history;

    QStackedLayout* m_layout;

    gui::WelcomeWidget* m_welcome_widget;
    gui::rebase::RebaseViewWidget* m_rebase_view;
    gui::rebase::RebaseSelectionWidget* m_rebase_select;

    std::optional<QString> m_save_file;
    std::optional<core::git::RepoSession> m_new_rebase_repo;

    app::Settings m_settings;
    app::conflict::ConflictEditor m_conflict_editor;
    QMap<QString, ShortcutAction> m_shortcuts;
    std::unique_ptr<QProcess> m_git_proc;

    /**
     * @brief Sets up the UI.
     */
    void setup();

    /**
     * @brief Sets up shortcuts.
     */
    void setupShortcuts();

    /**
     * @brief Toggles visibility of old commits.
     *
     * @param state Whether old commits should be hidden.
     */
    void hideOldCommits(bool state);

    /**
     * @brief Toggles visibility of result commits.
     *
     * @param state Whether result commits should be hidden.
     */
    void hideResultCommits(bool state);

    /**
     * @brief Loads the save file.
     */
    void loadSaveFile();

    /**
     * @brief Saves the todo file.
     *
     * @param insert_break Whether to insert a break command to todo file.
     */
    bool saveTodoFile(bool insert_break = false);

    /**
     * @brief Saves the save file.
     *
     * @param choose_file Whether to prompt for file selection.
     *
     * @details If the save file does not exist or @c choose_file is true,
     *          a file selection prompt is shown.
     */
    bool saveSaveFile(bool choose_file);

    /**
     * @brief Prompts to save changes if needed.
     *
     * @return User's save decision.
     */
    SaveStatus maybeSave();

    /**
     * @brief Prepares RebaseSelectionWidget
     */
    bool prepareRebaseSelection(core::git::RepoSession&& repo);

    /**
     * @brief Executes git interactive rebase and closes this app.
     */
    void startNewRebase(QString commit_id);

    /**
     * @brief Updates commits inside the RebaseSelectionWidget.
     */
    void updateRebaseSelection(const std::string& branch);

    core::Result<void> openRebase(const core::git::RebaseInfo& info, core::git::RepoSession&& repo);
    void openRebase(
        core::git::RepoSession&& repo,
        core::rebase::RebasePlan&& plan,
        core::conflict::ConflictManager&& conflicts,
        core::git::GitGraph<void>&& graph
    );

    void updateUndoActions();
};
