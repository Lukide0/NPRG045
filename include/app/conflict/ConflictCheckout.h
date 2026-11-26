#pragma once

#include "app/conflict/ConflictEditor.h"
#include "app/conflict/ConflictEditorSession.h"
#include "core/git/RepoSession.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/Result.h"

#include <cassert>
#include <filesystem>
#include <git2/oid.h>
#include <git2/repository.h>
#include <git2/types.h>

#include <memory>
#include <span>

#include <QByteArray>
#include <QDir>
#include <QTemporaryDir>
#include <string>
#include <string_view>
#include <vector>

namespace app::conflict {

enum class FileState {
    Unresolved,
    Resolved,
    ResolvedPending,
    ModifiedPending,
    DeletedPending,
};

struct PendingResolution {
    enum class Kind {
        Unchanged,
        Deleted,
        Contents,
    };

    ConflictFileId id;
    Kind kind = Kind::Unchanged;
    QByteArray contents;
};

struct ResolutionState {
    std::vector<PendingResolution> resolutions;
    core::git::index_t index;
};

class ConflictCheckout {
public:
    ConflictCheckout(QString base_dir);
    ~ConflictCheckout();

    ConflictCheckout(const ConflictCheckout&)            = delete;
    ConflictCheckout& operator=(const ConflictCheckout&) = delete;

    void setRepo(core::git::RepoSession* repo) {
        assert(!isActive());
        m_repo = repo;
    }

    [[nodiscard]] bool isActive() const;

    core::Result<void> begin(
        git_index* index,
        git_commit* commit_base,
        std::vector<core::rebase::ConflictFile> files,
        ConflictEditor::ConflictStyle style = ConflictEditor::ConflictStyle::DIFF3
    );
    core::Result<ResolutionState> collect() const;

    core::Result<void> end();

    [[nodiscard]] const QString& workdir() const { return m_work; }

    [[nodiscard]] std::span<const core::rebase::ConflictFile> files() const;

    core::Result<ConflictEditor::EditorValues> paths(const ConflictFileId& id);

    core::Result<std::set<std::string>> foreignChanges() const;

    [[nodiscard]] const core::rebase::ConflictFile* find(const ConflictFileId& id) const;

    core::Result<FileState> state(const ConflictFileId& id) const;

    [[nodiscard]] FileState stateAt(std::size_t i) const;

    static void removeStale(git_repository* repo);

private:
    static constexpr std::string_view s_prefix = "conflict-resolution";

    struct Head {
        git_oid oid;
        core::git::reference_t ref;
    };

    struct FileInit {
        bool unmerged  = false;
        bool had_entry = false;
        git_oid oid { };
    };

    QString m_base;
    core::git::RepoSession* m_repo = nullptr;
    bool m_active                  = false;

    std::unique_ptr<QTemporaryDir> m_root;
    QString m_work;

    std::vector<core::rebase::ConflictFile> m_files;
    std::vector<FileInit> m_init;
    core::git::index_t m_initial;

    struct {
        core::git::worktree_t worktree;
        core::git::repository_t repo;
        core::git::index_t index;
    } m_worktree;

    [[nodiscard]] QString resultPath(const std::string& relative_path) const;
    [[nodiscard]] std::size_t indexOf(const conflict::ConflictFileId& id) const;
    core::Result<void> teardown();

    core::Result<core::git::index_t> copyCurrentIndex() const;
};

}
