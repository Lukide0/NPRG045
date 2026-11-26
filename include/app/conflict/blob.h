#pragma once

#include "core/rebase/RebaseAnalysis.h"
#include "core/Result.h"

#include <filesystem>
#include <git2/types.h>
#include <optional>
#include <QString>

namespace app::conflict {

core::Result<void> writeBlob(git_repository* repo, const git_oid* oid, const QString& destination);
core::Result<void> writeBlob(git_repository* repo, const std::string& oid_str, const QString& destination);
core::Result<void> writeBlob(
    git_repository* repo,
    const std::optional<core::rebase::ConflictFile::Entry>& index_entry,
    const QString& destination
);

struct FileContent {
    bool exists;
    QByteArray content;
};

core::Result<FileContent> readFile(const QString& path, bool placeholder);
core::Result<FileContent> readFile(const std::filesystem::path& path, bool placeholder);

}
