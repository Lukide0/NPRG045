#include "app/conflict/ConflictEditorManager.h"
#include "app/conflict/ConflictEditor.h"
#include "app/conflict/ConflictEditorSession.h"
#include "core/git/error.h"
#include "core/git/RepoSession.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/Result.h"

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
#include <QString>
#include <QtTypes>

namespace app::conflict {

core::Result<void> writeBlob(git_repository* repo, const git_oid* oid, const QString& destination);

core::Result<void> writeBlob(git_repository* repo, const std::string& oid_str, const QString& destination);
core::Result<void>
writeBlob(git_repository* repo, const std::optional<git_index_entry>& index_entry, const QString& destination);

core::Result<void> writeMergePreview(
    git_repository* repo,
    const core::rebase::ConflictFile& conflict,
    const QString& destination,
    git_merge_file_flag_t flag
);

ConflictEditorManager::ConflictEditorManager(const ConflictEditor& editor, QObject* parent)
    : QObject(parent)
    , m_editor(editor) { }

core::Result<void>
ConflictEditorManager::open(const ConflictFileId& id, const core::rebase::ConflictFile& file, bool edit_resolution) {
    if (isOpen(id)) {
        return std::unexpected<core::Error>({
            .title   = "Could not open conflict editor",
            .message = "An editor is already open for this conflict.",
        });
    }

    auto temp_files = createTempConflictFiles(file, edit_resolution);
    if (!temp_files.has_value()) {
        return std::unexpected<core::Error>(temp_files.error());
    }

    auto editor = m_editor.createEditor(temp_files->files);
    if (!editor.has_value()) {
        return std::unexpected<core::Error>(editor.error());
    }

    auto* session = new ConflictEditorSession(id, std::move(editor).value(), std::move(temp_files).value(), this);

    // register before starting the process
    m_sessions.insert(id, session);

    connect(
        session,
        &ConflictEditorSession::resolved,
        this,
        [this, session](const QByteArray& content, ConflictResolutionType type) {
            const auto session_id = session->conflictId();
            // discared or invalid session
            if (m_sessions.value(session_id, nullptr) != session) {
                return;
            }

            emit resolutionReady(session_id, content, type);
        }
    );

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

core::Result<TemporaryConflictFiles>
ConflictEditorManager::createTempConflictFiles(const core::rebase::ConflictFile& file, bool edit_resolution) {
    auto directory = std::make_shared<QTemporaryDir>();

    if (!directory->isValid()) {
        return std::unexpected<core::Error>({
            .title   = "Cannot create temporary directory",
            .message = directory->errorString().toStdString(),
        });
    }

    const QString original_path = QString::fromStdString(file.path);
    const QString suffix        = QFileInfo(original_path).suffix();

    auto make_path
        = [&](const QString& name) { return directory->filePath(suffix.isEmpty() ? name : name + '.' + suffix); };

    ConflictEditor::EditorValues files {
        .base   = make_path("base"),
        .our    = make_path("our"),
        .their  = make_path("their"),
        .result = make_path("result"),
    };

    const auto conflict = std::to_array<std::pair<const std::optional<git_index_entry>&, const QString&>>({
        { file.ancestor, files.base  },
        { file.our,      files.our   },
        { file.their,    files.their },
    });

    for (auto&& [entry, path] : conflict) {
        core::Result<void> result = writeBlob(m_repo->repo(), entry, path);
        if (!result.has_value()) {
            return std::unexpected<core::Error>(result.error());
        }
    }

    git_merge_file_flag_t style;
    switch (m_editor.style()) {
    case ConflictEditor::ConflictStyle::DIFF3:
        style = GIT_MERGE_FILE_STYLE_DIFF3;
        break;
    case ConflictEditor::ConflictStyle::ZDIFF3:
        style = GIT_MERGE_FILE_STYLE_ZDIFF3;
        break;
    case ConflictEditor::ConflictStyle::MERGE:
        style = GIT_MERGE_FILE_STYLE_MERGE;
        break;
    }

    core::Result<void> result;

    // edit resolution instead
    if (file.resolution.has_value() && edit_resolution) {
        // resolved by removing file
        if (file.resolution->empty()) {
            result = { };
        } else {
            result = writeBlob(m_repo->repo(), *file.resolution, files.result);
        }
    } else {
        result = writeMergePreview(m_repo->repo(), file, files.result, style);
    }

    if (!result.has_value()) {
        return std::unexpected<core::Error>(result.error());
    }

    return TemporaryConflictFiles {
        .directory = std::move(directory),
        .files     = std::move(files),
    };
}

core::Result<void> writeMergePreview(
    git_repository* repo,
    const core::rebase::ConflictFile& conflict,
    const QString& destination,
    git_merge_file_flag_t flag
) {
    git_index_entry ancestor { };
    git_index_entry our { };
    git_index_entry their { };

    const git_index_entry* ancestor_ptr = nullptr;
    const git_index_entry* our_ptr      = nullptr;
    const git_index_entry* their_ptr    = nullptr;

    if (conflict.ancestor) {
        ancestor      = *conflict.ancestor;
        ancestor.path = conflict.path.c_str();
        ancestor_ptr  = &ancestor;
    }

    if (conflict.our) {
        our      = *conflict.our;
        our.path = conflict.path.c_str();
        our_ptr  = &our;
    }

    if (conflict.their) {
        their      = *conflict.their;
        their.path = conflict.path.c_str();
        their_ptr  = &their;
    }

    git_merge_file_options options = GIT_MERGE_FILE_OPTIONS_INIT;
    options.flags                  = flag;
    options.ancestor_label         = "ancestor";
    options.our_label              = "ours";
    options.their_label            = "theirs";

    core::git::merge_file_result_t merged { { } };
    if (git_merge_file_from_index(&merged, repo, ancestor_ptr, our_ptr, their_ptr, &options) != 0) {
        return std::unexpected<core::Error>({
            .title   = "Failed to merge conflict file",
            .message = core::git::get_last_error(),
        });
    }

    QFile file(destination);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return std::unexpected<core::Error>({
            .title   = "Failed to create conflict file",
            .message = file.errorString().toStdString(),
        });
    }

    const auto& result = merged.get();

    if (result.len > 0 && file.write(result.ptr, static_cast<qint64>(result.len)) != static_cast<qint64>(result.len)) {
        return std::unexpected<core::Error>({
            .title   = "Failed to write conflict file",
            .message = file.errorString().toStdString(),
        });
    }

    if (!file.flush()) {
        return std::unexpected<core::Error>({
            .title   = "Failed to write conflict file",
            .message = file.errorString().toStdString(),
        });
    }

    return { };
}

core::Result<void> writeBlob(git_repository* repo, const git_oid* oid, const QString& destination) {
    QFile file(destination);

    if (!file.open(QIODevice::WriteOnly)) {
        return std::unexpected<core::Error>({
            .title   = "Cannot create temporary file",
            .message = file.errorString().toStdString(),
        });
    }

    // deleted file
    if (oid == nullptr || git_oid_is_zero(oid) != 0) {
        return { };
    }

    git_blob* raw_blob = nullptr;
    const int error    = git_blob_lookup(&raw_blob, repo, oid);

    if (error != 0) {
        return std::unexpected<core::Error>({
            .title   = "Cannot read Git object",
            .message = core::git::get_last_error(),
        });
    }

    core::git::blob_t blob { raw_blob };

    const auto size     = git_blob_rawsize(blob.get());
    const auto* content = static_cast<const char*>(git_blob_rawcontent(blob.get()));

    if (size > 0 && file.write(content, static_cast<qint64>(size)) != static_cast<qint64>(size)) {
        return std::unexpected<core::Error>({
            .title   = "Cannot read Git object",
            .message = "The Git blob has no accessible contents",
        });
    }

    if (!file.flush()) {
        return std::unexpected<core::Error>({
            .title   = "Cannot write temporary file",
            .message = file.errorString().toStdString(),
        });
    }

    return { };
}

core::Result<void>
writeBlob(git_repository* repo, const std::optional<git_index_entry>& index_entry, const QString& destination) {
    const git_oid* oid = nullptr;
    if (index_entry.has_value()) {
        oid = &index_entry->id;
    }

    return writeBlob(repo, oid, destination);
}

core::Result<void> writeBlob(git_repository* repo, const std::string& oid_str, const QString& destination) {
    git_oid oid;

    if (git_oid_fromstrn(&oid, oid_str.data(), oid_str.size()) != 0) {
        return std::unexpected<core::Error>({
            .title   = "Cannot parse Git oid",
            .message = core::git::get_last_error(),
        });
    }

    return writeBlob(repo, &oid, destination);
}

}
