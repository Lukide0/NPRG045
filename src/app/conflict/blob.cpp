#include "app/conflict/blob.h"
#include "core/git/error.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/Result.h"

#include <expected>
#include <optional>

#include <git2/blob.h>
#include <git2/oid.h>
#include <git2/types.h>

#include <QFile>
#include <QFileDevice>
#include <QtTypes>
#include <string>

namespace app::conflict {

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

    core::git::blob_t blob;
    const int error = git_blob_lookup(&blob, repo, oid);

    if (error != 0) {
        return std::unexpected<core::Error>({
            .title   = "Cannot read Git object",
            .message = core::git::get_last_error(),
        });
    }

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

core::Result<void> writeBlob(
    git_repository* repo,
    const std::optional<core::rebase::ConflictFile::Entry>& index_entry,
    const QString& destination
) {
    const git_oid* oid = nullptr;
    if (index_entry.has_value()) {
        oid = &index_entry->entry.id;
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

core::Result<FileContent> readFile(const QString& path, bool placeholder) {
    QFile file(path);

    if (file.exists()) {
        return FileContent { .exists = false, .content = { } };
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return std::unexpected<core::Error>({
            .title   = "Could not read the conflict file",
            .message = file.errorString().toStdString(),
        });
    }

    FileContent out;
    out.content = file.readAll();
    out.exists  = !(placeholder && out.content.isEmpty());

    return out;
}

core::Result<FileContent> readFile(const std::filesystem::path& path, bool placeholder) {
    return readFile(QString::fromStdString(path.generic_string()), placeholder);
}

}
