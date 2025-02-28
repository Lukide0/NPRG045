#include "core/state/CommandHistory.h"
#include "core/state/Command.h"
#include <cstddef>
#include <memory>
#include <utility>

namespace core::state {

CommandHistory g_history;

bool CommandHistory::undo() {
    if (!canUndo()) {
        return false;
    }

    m_commands[m_index]->undo();
    m_index -= 1;

    if (m_undo != nullptr) {
        m_undo->setEnabled(canUndo());
    }

    if (m_redo != nullptr) {
        m_redo->setEnabled(true);
    }

    return true;
}

bool CommandHistory::redo() {
    if (!canRedo()) {
        return false;
    }

    m_index += 1;
    m_commands[m_index]->execute();

    if (m_redo != nullptr) {
        m_redo->setEnabled(canRedo());
    }

    if (m_undo != nullptr) {
        m_undo->setEnabled(true);
    }

    return true;
}

void CommandHistory::add(std::unique_ptr<Command>&& cmd) {
    m_index += 1;

    if (static_cast<std::size_t>(m_index) != m_commands.size()) {
        m_commands.resize(m_index);
    }

    m_commands.emplace_back(std::move(cmd));

    if (m_undo != nullptr) {
        m_undo->setEnabled(true);
    }
}

void CommandHistory::clear() {
    m_commands.clear();
    m_index = -1;

    m_undo->setEnabled(false);
    m_redo->setEnabled(false);
}

void CommandHistory::setUndo(QAction* undo) { m_undo = undo; }

void CommandHistory::setRedo(QAction* redo) { m_redo = redo; }

bool CommandHistory::CanUndo() { return g_history.canUndo(); }

bool CommandHistory::CanRedo() { return g_history.canRedo(); }

bool CommandHistory::Undo() { return g_history.undo(); }

bool CommandHistory::Redo() { return g_history.redo(); }

void CommandHistory::Add(std::unique_ptr<Command>&& cmd) { g_history.add(std::move(cmd)); }

void CommandHistory::Clear() { g_history.clear(); }

void CommandHistory::SetUndo(QAction* undo) { g_history.setUndo(undo); }

void CommandHistory::SetRedo(QAction* redo) { g_history.setRedo(redo); }

}
