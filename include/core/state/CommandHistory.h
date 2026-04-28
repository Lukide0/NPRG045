#pragma once

#include "Command.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <QAction>
#include <vector>

namespace core::state {

/**
 * @brief Manages command execution history.
 */
class CommandHistory {
public:
    /**
     * @brief Checks if undo is available.
     */
    [[nodiscard]] bool canUndo() const { return m_index >= 0; }

    /**
     * @brief Checks if redo is available.
     */
    [[nodiscard]] bool canRedo() const { return static_cast<std::size_t>(m_index + 1) < m_commands.size(); }

    /**
     * @brief Undoes the last executed command.
     */
    bool undo();

    /**
     * @brief Redoes the last undone command.
     */
    bool redo();

    /**
     * @brief Adds a new command to history.
     *
     * @param cmd Command to add.
     */
    void add(std::unique_ptr<Command>&& cmd);

    /**
     * @brief Clears all command history.
     */
    void clear();

    /**
     * @brief Checks if current state matches saved state.
     */
    [[nodiscard]] bool isSaved() const { return m_index == m_saved_index; }

    /**
     * @brief Marks current state as saved.
     */
    void save() {
        if (m_index != -1) {
            m_saved_index = m_index;
        }
    }

    /**
     * @brief Sets undo QAction reference.
     *
     * @param undo Undo action.
     */
    void setUndo(QAction* undo);

    /**
     * @brief Sets redo QAction reference.
     *
     * @param redo Redo action.
     */
    void setRedo(QAction* redo);

    static bool CanUndo();
    static bool CanRedo();
    static bool Undo();
    static bool Redo();
    static void Add(std::unique_ptr<Command>&& cmd);
    static void Clear();
    static void Save();
    static bool IsSaved();
    static void SetUndo(QAction* undo);
    static void SetRedo(QAction* redo);

private:
    std::int32_t m_index       = -1;
    std::int32_t m_saved_index = -1;

    std::vector<std::unique_ptr<Command>> m_commands;
    QAction* m_undo;
    QAction* m_redo;
};

}
