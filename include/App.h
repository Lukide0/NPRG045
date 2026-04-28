#pragma once

#include "gui/widget/RebaseViewWidget.h"
#include "gui/widget/WelcomeWidget.h"

#include <cassert>
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
#include <QSettings>
#include <QString>

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

    ~App() override { git_libgit2_shutdown(); }

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
     * @brief Updates commit graph.
     */
    static void updateGraph();

    /**
     * @brief Updates actions list, conflict list and commit graph.
     */
    static void updateActions();

    /**
     * @brief Updates the conflict list and markers.
     *
     * @param start The first action to update. If @c start is @c nullptr,
     *              the first action is used.
     *
     * @details If @c start is @c nullptr, the update begins from the first action.
     */
    static void updateConflicts(action::Action* start = nullptr);

    /**
     * @brief Returns the rebase view widget.
     */
    static gui::widget::RebaseViewWidget* getRebaseViewWidget();

    /**
     * @brief Returns the current repository path.
     */
    static const std::string& getRepoPath();

    /**
     * @brief Returns application settings.
     */
    static QSettings getSettings() { return { QSettings::IniFormat, QSettings::UserScope, "gitshuffle" }; }

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

    /**
     * @brief Indicates whether the application was started with a path argument.
     */
    bool m_cli_start = false;

    QAction* m_repo_open;
    QAction* m_load_save;

    std::string m_repo_path;
    std::string m_rebase_head;
    std::string m_rebase_onto;

    QHBoxLayout* m_layout;
    gui::widget::RebaseViewWidget* m_rebase_view;
    gui::widget::WelcomeWidget* m_welcome_widget;

    core::git::repository_t m_repo;

    std::optional<QString> m_save_file;

    QMap<QString, ShortcutAction> m_shortcuts;

    /**
     * @brief Sets up the UI.
     */
    void setup();

    /**
     * @brief Sets up shortcuts.
     */
    void setupShortcuts();

    /**
     * @brief Processes the Git todo file and updates the rebase view widget.
     *
     * @return True if processing succeeded, false otherwise.
     */
    bool loadRebase();

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
    bool loadSaveFile();

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
};
