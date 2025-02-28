#pragma once

#include "Command.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <QAction>
#include <vector>

namespace core::state {

class CommandHistory {
public:
    [[nodiscard]] bool canUndo() const { return m_index >= 0; }

    [[nodiscard]] bool canRedo() const { return static_cast<std::size_t>(m_index + 1) < m_commands.size(); }

    bool undo();
    bool redo();

    void add(std::unique_ptr<Command>&& cmd);
    void clear();

    void setUndo(QAction* undo);
    void setRedo(QAction* redo);

    static bool CanUndo();
    static bool CanRedo();
    static bool Undo();
    static bool Redo();
    static void Add(std::unique_ptr<Command>&& cmd);
    static void Clear();
    static void SetUndo(QAction* undo);
    static void SetRedo(QAction* redo);

private:
    std::int32_t m_index = -1;
    std::vector<std::unique_ptr<Command>> m_commands;
    QAction* m_undo;
    QAction* m_redo;
};

}
