#include "app/conflict/ConflictEditorManager.h"
#include "app/conflict/ConflictEditor.h"
#include "app/conflict/ConflictEditorSession.h"
#include "core/git/error.h"
#include "core/git/RepoSession.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/Result.h"
#include "logging/Log.h"

#include <expected>
#include <git2/merge.h>
#include <memory>
#include <optional>
#include <utility>

#include <git2/blob.h>
#include <git2/index.h>
#include <git2/oid.h>
#include <git2/types.h>

#include <QFile>
#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <QtTypes>

namespace app::conflict {

QString checkout_root() {
    const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return QDir(cachePath).filePath("git-worktrees");
}

ConflictEditorManager::ConflictEditorManager(const ConflictEditor& editor, QObject* parent)
    : QObject(parent)
    , m_editor(editor)
    , m_checkout(checkout_root()) { }

core::Result<void> ConflictEditorManager::open(const ConflictFileId& id) {
    if (isOpen(id)) {
        return std::unexpected<core::Error>({
            .title   = "Could not open conflict editor",
            .message = "An editor is already open for this conflict.",
        });
    }

    auto temp_files = m_checkout.paths(id);
    if (!temp_files.has_value()) {
        return std::unexpected<core::Error>(temp_files.error());
    }

    auto editor = m_editor.createEditor(temp_files.value());
    if (!editor.has_value()) {
        return std::unexpected<core::Error>(editor.error());
    }

    auto* session = new ConflictEditorSession(id, std::move(editor).value(), std::move(temp_files).value(), this);

    // register before starting the process
    m_sessions.insert(id, session);

    connect(session, &ConflictEditorSession::closedEditor, this, [this, session]() {
        const auto session_id = session->conflictId();
        // discared or invalid session
        if (m_sessions.value(session_id, nullptr) != session) {
            return;
        }

        const auto status = m_checkout.state(session_id);
        if (!status.has_value()) {
            const auto err = status.error();
            LOG_ERROR("Could not read the file state: {}", err.message);
            return;
        }

        emit editorClosed(session_id, status.value());
    });

    connect(session, &ConflictEditorSession::failed, this, [this, session](const QString& message) {
        const auto session_id = session->conflictId();

        if (m_sessions.value(session_id, nullptr) != session) {
            return;
        }

        emit editorFailed(session_id, message);
    });

    connect(session, &ConflictEditorSession::stopped, this, [this, session] {
        const auto session_id = session->conflictId();

        if (m_sessions.value(session_id, nullptr) == session) {
            m_sessions.remove(session_id);
        }

        session->disconnect(this);
        session->deleteLater();
    });

    // start the process
    session->start();

    return { };
}

void ConflictEditorManager::discardOne(const ConflictFileId& id) {
    auto* session = m_sessions.value(id, nullptr);
    if (session == nullptr) {
        return;
    }

    m_sessions.remove(id);

    session->disconnect(this);
    session->discard();
    session->deleteLater();
}

void ConflictEditorManager::discardExcept(const QSet<ConflictFileId>& ids) {
    const auto open = m_sessions.keys();
    for (const auto& id : open) {
        if (!ids.contains(id)) {
            discardOne(id);
        }
    }
}

void ConflictEditorManager::discardAll() {
    for (auto* session : m_sessions) {
        session->discard();
    }
    m_sessions.clear();
}

}
