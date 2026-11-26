#pragma once

#include "app/conflict/ConflictCheckout.h"
#include "app/conflict/ConflictEditor.h"
#include "app/conflict/ConflictEditorSession.h"
#include "core/git/RepoSession.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/Result.h"
#include <git2/oid.h>
#include <git2/types.h>
#include <QHash>
#include <QObject>

namespace app::conflict {

class ConflictEditorManager : public QObject {
    Q_OBJECT

public:
    ConflictEditorManager(const ConflictEditor& editor, QObject* parent = nullptr);

    void setRepo(core::git::RepoSession* repo) {
        discardAll();
        m_repo = repo;
        m_checkout.setRepo(repo);
    }

    core::Result<void>
    resolutionOpen(git_index* conflict, git_commit* base, std::vector<core::rebase::ConflictFile> files) {
        return m_checkout.begin(conflict, base, std::move(files), m_editor.style());
    }

    core::Result<void> resolutionEnd() { return m_checkout.end(); }

    core::Result<ResolutionState> collectResolutions() const { return m_checkout.collect(); }

    [[nodiscard]] std::span<const core::rebase::ConflictFile> files() const { return m_checkout.files(); }

    [[nodiscard]] const core::rebase::ConflictFile* file(const ConflictFileId& id) const { return m_checkout.find(id); }

    core::Result<std::set<std::string>> foreignChanges() const { return m_checkout.foreignChanges(); }

    [[nodiscard]] QString workdir() const { return m_checkout.workdir(); }

    void iterateFiles(std::function<void(const core::rebase::ConflictFile&, FileState)> callback) {
        const auto files = m_checkout.files();
        for (std::size_t i = 0; i < files.size(); ++i) {
            const auto& file = files[i];
            const auto state = m_checkout.stateAt(i);

            callback(file, state);
        }
    }

    core::Result<void> open(const ConflictFileId& id);

    [[nodiscard]] bool isOpen(const ConflictFileId& id) const { return m_sessions.contains(id); }

    [[nodiscard]] bool isOpenResolution() const { return m_checkout.isActive(); }

    [[nodiscard]] bool hasOpenEditors() const { return !m_sessions.isEmpty(); }

    [[nodiscard]] QList<ConflictFileId> openIds() const { return m_sessions.keys(); }

    void discardOne(const ConflictFileId& id);
    void discardExcept(const QSet<ConflictFileId>& ids);
    void discardAll();

signals:
    void editorClosed(ConflictFileId id, FileState type);
    void editorFailed(ConflictFileId id, QString message);

private:
    core::git::RepoSession* m_repo = nullptr;
    const ConflictEditor& m_editor;

    ConflictCheckout m_checkout;

    QHash<ConflictFileId, ConflictEditorSession*> m_sessions;
};
}
