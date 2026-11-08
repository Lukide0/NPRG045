#pragma once

#include "app/conflict/ConflictEditor.h"
#include "app/conflict/ConflictEditorSession.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/RepoSession.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebaseSession.h"
#include <git2/oid.h>
#include <git2/types.h>
#include <QHash>
#include <QObject>
#include <span>

namespace app::conflict {

class ConflictEditorManager : public QObject {
    Q_OBJECT

public:
    ConflictEditorManager(const ConflictEditor& editor, QObject* parent = nullptr);

    void setRepo(core::git::RepoSession* repo) {
        discardAll();
        m_repo = repo;
    }

    core::Result<void>
    open(const ConflictFileId& id, const core::rebase::ConflictFile& file, bool edit_resolution = false);

    [[nodiscard]] bool isOpen(const ConflictFileId& id) const { return m_sessions.contains(id); }

    [[nodiscard]] bool hasOpenEditors() const { return !m_sessions.isEmpty(); }

    [[nodiscard]] QList<ConflictFileId> openIds() const { return m_sessions.keys(); }

    void discardOne(const ConflictFileId& id);
    void discardExcept(const QSet<ConflictFileId>& ids);
    void discardAll();

signals:
    void resolutionReady(ConflictFileId id, QByteArray content, ConflictResolutionType type);
    void editorFailed(ConflictFileId id, QString message);

private:
    core::Result<TemporaryConflictFiles>
    createTempConflictFiles(const core::rebase::ConflictFile& file, bool edit_resolution);

    core::git::RepoSession* m_repo = nullptr;
    const ConflictEditor& m_editor;

    QHash<ConflictFileId, ConflictEditorSession*> m_sessions;
};
}
