#include "app/conflict/ConflictCheckout.h"
#include "app/conflict/blob.h"
#include "app/conflict/ConflictEditor.h"
#include "app/conflict/ConflictEditorSession.h"
#include "core/conflict/conflict_iterator.h"
#include "core/git/error.h"
#include "core/git/RepoSession.h"
#include "core/git/types.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/Result.h"

#include <cstddef>
#include <expected>
#include <git2/blob.h>
#include <git2/branch.h>
#include <git2/worktree.h>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <git2/checkout.h>
#include <git2/diff.h>
#include <git2/errors.h>
#include <git2/index.h>
#include <git2/object.h>
#include <git2/oid.h>
#include <git2/refs.h>
#include <git2/repository.h>
#include <git2/reset.h>
#include <git2/revparse.h>
#include <git2/status.h>
#include <git2/tree.h>
#include <git2/types.h>

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QString>

namespace app::conflict {

core::Result<void> copy_index_into(git_index* dst, git_index* src) {
    if (git_index_clear(dst) != 0) {
        return std::unexpected<core::Error>({
            .title   = "Could not clear the index",
            .message = core::git::get_last_error(),
        });
    }

    std::size_t entries = git_index_entrycount(src);
    for (std::size_t i = 0; i < entries; ++i) {
        const git_index_entry* e = git_index_get_byindex(src, i);
        if (e == nullptr) {
            continue;
        }

        if (git_index_entry_stage(e) == 0 && git_index_add(dst, e) != 0) {
            return std::unexpected<core::Error>({
                .title   = "Could not copy an index entry",
                .message = core::git::get_last_error(),
            });
        }
    }

    core::Result<void> res;
    const bool r = core::conflict::iterate(src, [&](core::conflict::entry_data_t data) -> bool {
        if (git_index_conflict_add(dst, data.ancestor, data.our, data.their) != 0) {
            res = std::unexpected<core::Error>({
                .title   = "Could not copy a conflict",
                .message = core::git::get_last_error(),
            });
            return false;
        }

        return true;
    });

    if (!res.has_value()) {
        return res;
    }

    if (!r) {
        return std::unexpected<core::Error>({
            .title   = "Could not iterate conflicts",
            .message = core::git::get_last_error(),
        });
    }

    const bool differs = git_index_has_conflicts(src) != git_index_has_conflicts(dst)
        || git_index_entrycount(src) != git_index_entrycount(dst);

    if (differs) {
        return std::unexpected<core::Error>({
            .title   = "Could not copy the index",
            .message = "The copied index differs from the source index",
        });
    }

    return { };
}

int prune_worktree(git_worktree* wt) {
    git_worktree_prune_options opts = GIT_WORKTREE_PRUNE_OPTIONS_INIT;
    opts.flags = GIT_WORKTREE_PRUNE_VALID | GIT_WORKTREE_PRUNE_LOCKED | GIT_WORKTREE_PRUNE_WORKING_TREE;
    return git_worktree_prune(wt, &opts);
}

QByteArray hash_of(QByteArrayView in) { return QCryptographicHash::hash(in, QCryptographicHash::Sha256); }

std::string signature_oid(const git_index_entry* e) {
    if (e == nullptr) {
        return "-";
    }

    return core::git::format_oid_to_str<core::git::OID_SIZE>(&e->id);
}

std::string signature(const git_index_entry* ancestor, const git_index_entry* our, const git_index_entry* their) {
    return signature_oid(ancestor) + ":" + signature_oid(our) + ":" + signature_oid(their);
}

std::string signature(const core::rebase::ConflictFile file) {
    const git_index_entry* a = file.ancestor.has_value() ? &file.ancestor->entry : nullptr;
    const git_index_entry* o = file.our.has_value() ? &file.our->entry : nullptr;
    const git_index_entry* t = file.their.has_value() ? &file.their->entry : nullptr;

    return signature(a, o, t);
}

ConflictCheckout::ConflictCheckout(QString base_dir)
    : m_base(std::move(base_dir)) { }

ConflictCheckout::~ConflictCheckout() { auto _ = teardown(); }

[[nodiscard]] bool ConflictCheckout::isActive() const { return m_active; }

core::Result<void> ConflictCheckout::begin(
    git_index* index,
    git_commit* commit_base,
    std::vector<core::rebase::ConflictFile> files,
    ConflictEditor::ConflictStyle style
) {
    if (isActive() || m_worktree.worktree != nullptr) {
        return std::unexpected<core::Error>({
            .title   = "Could not start conflict resolution",
            .message = "A conflict resolution is already active",
        });
    }

    if (index == nullptr) {
        return std::unexpected<core::Error>({
            .title   = "Could not start conflict resolution",
            .message = "The analysis index is not available",
        });
    }

    if (!QDir().mkpath(m_base)) {
        return std::unexpected<core::Error>({
            .title   = "Could not start conflict resolution",
            .message = "Could not create " + m_base.toStdString(),
        });
    }

    const QString root_path = QDir(m_base).filePath(QString::fromUtf8(s_prefix.data(), s_prefix.size()));
    m_root                  = std::make_unique<QTemporaryDir>(root_path);

    if (!m_root->isValid()) {
        return std::unexpected<core::Error>({
            .title   = "Cannot create temporary directory",
            .message = m_root->errorString().toStdString(),
        });
    }

    m_work                 = m_root->filePath(QStringLiteral("work"));
    const std::string name = QFileInfo(m_root->path()).fileName().toStdString();

    core::git::reference_t branch;

    auto cleanup = [&](core::Error e) {
        auto _ = teardown();
        if (branch != nullptr) {
            branch.destroy();
        }

        return std::unexpected<core::Error>(e);
    };

    if (git_branch_create(&branch, m_repo->repo(), name.c_str(), commit_base, /*force=*/0) != 0) {
        return cleanup(
            {
                .title   = "Could not create the worktree branch",
                .message = core::git::get_last_error(),
            }
        );
    }

    // create worktree
    {

        git_worktree_add_options opts = GIT_WORKTREE_ADD_OPTIONS_INIT;
        opts.lock                     = 1;
        opts.ref                      = branch;

        git_worktree* worktree  = nullptr;
        const auto worktree_dir = m_work.toUtf8().toStdString();
        if (git_worktree_add(&worktree, m_repo->repo(), name.c_str(), worktree_dir.c_str(), &opts) != 0) {
            return cleanup(
                {
                    .title   = "Could not create the worktree",
                    .message = core::git::get_last_error(),
                }
            );
        }
        m_worktree.worktree.reset(worktree);

        git_repository* worktree_repo = nullptr;
        if (git_repository_open_from_worktree(&worktree_repo, m_worktree.worktree) != 0) {
            return cleanup(
                {
                    .title   = "Could not open the worktree",
                    .message = core::git::get_last_error(),
                }
            );
        }
        m_worktree.repo.reset(worktree_repo);

        if (git_repository_set_head_detached(m_worktree.repo, git_commit_id(commit_base)) != 0) {
            return cleanup(
                {
                    .title   = "Could not detach the worktree HEAD",
                    .message = core::git::get_last_error(),
                }
            );
        }

        if (git_branch_delete(branch) != 0) {
            return cleanup(
                {
                    .title   = "Could not delete the temporary branch",
                    .message = core::git::get_last_error(),
                }
            );
        }
        branch.destroy();
    }

    // checkout index with conflict
    {
        git_index* raw_index = nullptr;
        if (git_repository_index(&raw_index, m_worktree.repo) != 0) {
            return cleanup(
                {
                    .title   = "Could not open the worktree index",
                    .message = core::git::get_last_error(),
                }
            );
        }
        m_worktree.index.reset(raw_index);

        const auto r = copy_index_into(m_worktree.index, index);
        if (!r.has_value()) {
            return cleanup(r.error());
        }

        unsigned int checkout_style;
        switch (style) {
        case ConflictEditor::ConflictStyle::DIFF3:
            checkout_style = GIT_CHECKOUT_CONFLICT_STYLE_DIFF3;
            break;
        case ConflictEditor::ConflictStyle::ZDIFF3:
            checkout_style = GIT_CHECKOUT_CONFLICT_STYLE_ZDIFF3;
            break;
        case ConflictEditor::ConflictStyle::MERGE:
            checkout_style = GIT_CHECKOUT_CONFLICT_STYLE_MERGE;
            break;
        }

        git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
        opts.checkout_strategy    = GIT_CHECKOUT_FORCE | GIT_CHECKOUT_ALLOW_CONFLICTS | checkout_style;
        opts.our_label            = "onto";
        opts.their_label          = "picked";
        opts.ancestor_label       = "base";

        if (git_checkout_index(m_worktree.repo, m_worktree.index, &opts) != 0) {
            return cleanup(
                {
                    .title   = "Could not checkout the conflict",
                    .message = core::git::get_last_error(),
                }
            );
        }

        if (git_index_write(m_worktree.index) != 0) {
            return cleanup(
                {
                    .title   = "Could not write the worktree index",
                    .message = core::git::get_last_error(),
                }
            );
        }
    }

    git_index* raw_initial = nullptr;
    if (git_index_new(&raw_initial) != 0) {
        return cleanup(
            {
                .title   = "Could not create the index snapshot",
                .message = core::git::get_last_error(),
            }
        );
    }
    m_initial.reset(raw_initial);

    auto r = copy_index_into(m_initial, m_worktree.index);
    if (!r.has_value()) {
        return cleanup(r.error());
    }

    m_files = std::move(files);
    m_init.assign(m_files.size(), FileInit { });

    for (std::size_t i = 0; i < m_files.size(); ++i) {
        const char* path = m_files[i].path.c_str();

        const git_index_entry* a = nullptr;
        const git_index_entry* o = nullptr;
        const git_index_entry* t = nullptr;

        m_init[i].unmerged = git_index_conflict_get(&a, &o, &t, m_worktree.index, path) == 0;

        const git_index_entry* e = git_index_get_bypath(m_worktree.index, path, 0);
        if (e != nullptr) {
            m_init[i].had_entry = true;
            m_init[i].oid       = e->id;
        }
    }

    m_active = true;
    return { };
}

core::Result<void> ConflictCheckout::end() {
    if (!isActive()) {
        return std::unexpected<core::Error>({
            .title   = "Could not end the conflict resolution",
            .message = "No conflict resolution is active",
        });
    }

    return teardown();
}

core::Result<void> ConflictCheckout::teardown() {

    m_active = false;
    m_files.clear();

    m_initial.destroy();
    m_init.clear();

    m_worktree.index.destroy();
    m_worktree.repo.destroy();

    core::Result<void> result;
    if (m_worktree.worktree != nullptr) {
        if (prune_worktree(m_worktree.worktree) != 0) {
            result = std::unexpected<core::Error>({
                .title   = "Could not remove the conflict worktree",
                .message = core::git::get_last_error(),
            });
        }

        m_worktree.worktree.destroy();
    }

    m_root.reset();
    return result;
}

FileState ConflictCheckout::stateAt(std::size_t i) const {
    const char* path = m_files[i].path.c_str();
    const auto& init = m_init[i];

    const git_index_entry* a = nullptr;
    const git_index_entry* o = nullptr;
    const git_index_entry* t = nullptr;

    if (git_index_conflict_get(&a, &o, &t, m_worktree.index.get(), path) == 0) {
        return FileState::Unresolved;
    }

    const git_index_entry* e = git_index_get_bypath(m_worktree.index.get(), path, 0);

    // was resolved
    if (!init.unmerged) {
        const bool same = (e == nullptr && !init.had_entry)
            || (e != nullptr && init.had_entry && git_oid_equal(&e->id, &init.oid) != 0);

        if (same) {
            return FileState::Resolved;
        }

        return e != nullptr ? FileState::ModifiedPending : FileState::DeletedPending;
    }

    return e != nullptr ? FileState::ResolvedPending : FileState::DeletedPending;
}

core::Result<FileState> ConflictCheckout::state(const ConflictFileId& id) const {
    const auto i = indexOf(id);
    if (!isActive() || i >= m_files.size()) {
        return std::unexpected<core::Error>({
            .title   = "Could not read the file state",
            .message = "Unknown conflict file",
        });
    }

    if (git_index_read(m_worktree.index.get(), /*force=*/1) != 0) {
        return std::unexpected<core::Error>({
            .title   = "Could not read the worktree index",
            .message = core::git::get_last_error(),
        });
    }

    return stateAt(i);
}

core::Result<std::set<std::string>> ConflictCheckout::foreignChanges() const {
    if (!m_active) {
        return std::unexpected<core::Error>({
            .title   = "Could not inspect the worktree",
            .message = "No conflict resolution is active",
        });
    }

    if (git_index_read(m_worktree.index.get(), 1) != 0) {
        return std::unexpected<core::Error>({
            .title   = "Could not read the worktree index",
            .message = core::git::get_last_error(),
        });
    }

    std::set<std::string> conflict_paths;
    for (const auto& f : m_files) {
        conflict_paths.insert(f.path);
    }

    std::set<std::string> changed;
    auto compare = [&](git_index* from, git_index* to) {
        const std::size_t from_count = git_index_entrycount(from);

        for (std::size_t i = 0; i < from_count; ++i) {
            const git_index_entry* e = git_index_get_byindex(from, i);

            if (e == nullptr || e->path == nullptr || git_index_entry_stage(e) != 0
                || conflict_paths.contains(e->path)) {
                continue;
            }

            const git_index_entry* other = git_index_get_bypath(to, e->path, 0);
            if (other == nullptr || git_oid_equal(&other->id, &e->id) == 0 || other->mode != e->mode) {
                changed.insert(e->path);
            }
        }
    };

    compare(m_worktree.index.get(), m_initial.get()); // modified or added
    compare(m_initial.get(), m_worktree.index.get()); // removed

    return changed;
}

void ConflictCheckout::removeStale(git_repository* repo) {
    git_strarray names { };

    if (git_worktree_list(&names, repo) != 0) {
        return;
    }

    for (std::size_t i = 0; i < names.count; ++i) {
        const std::string_view name = names.strings[i];

        if (!name.starts_with(s_prefix)) {
            continue;
        }

        git_worktree* wt = nullptr;
        if (git_worktree_lookup(&wt, repo, names.strings[i]) != 0) {
            continue;
        }

        prune_worktree(wt);
        git_worktree_free(wt);

        // remove the temporary branch
        git_reference* ref = nullptr;
        if (git_branch_lookup(&ref, repo, names.strings[i], GIT_BRANCH_LOCAL) == 0) {
            git_branch_delete(ref);
            git_reference_free(ref);
        }
    }

    git_strarray_dispose(&names);
}

std::span<const core::rebase::ConflictFile> ConflictCheckout::files() const { return m_files; }

core::Result<ConflictEditor::EditorValues> ConflictCheckout::paths(const ConflictFileId& id) {
    if (!isActive()) {
        return std::unexpected<core::Error>({
            .title   = "Could not open the conflict",
            .message = "No conflict resolution is active",
        });
    }

    const auto i = indexOf(id);
    if (i >= m_files.size()) {
        return std::unexpected<core::Error>({
            .title   = "Could not open the conflict",
            .message = "Unknown conflict file",
        });
    }

    const auto& file = m_files[i];

    const auto result = resultPath(file.path);
    const QFileInfo info { result };
    if (!QDir().mkpath(info.absolutePath())) {
        return std::unexpected<core::Error>({
            .title   = "Could not prepare the result file",
            .message = "Could not create the directory for " + result.toStdString(),
        });
    }

    if (!info.exists()) {
        QFile placeholder(result);

        if (!placeholder.open(QIODevice::WriteOnly)) {
            return std::unexpected<core::Error>({
                .title   = "Could not create the result file",
                .message = placeholder.errorString().toStdString(),
            });
        }
    }

    const QString dir = m_root->filePath(QStringLiteral("side/%1").arg(i));
    if (!QDir().mkpath(dir)) {
        return std::unexpected<core::Error>({
            .title   = "Could not prepare the conflict files",
            .message = "Could not create " + dir.toStdString(),
        });
    }

    const QString suffix = QFileInfo(QString::fromStdString(file.path)).suffix();

    auto make_path
        = [&](const QString& name) { return QDir(dir).filePath(suffix.isEmpty() ? name : name + '.' + suffix); };

    ConflictEditor::EditorValues files {
        .base        = make_path("base"),
        .our         = make_path("our"),
        .their       = make_path("their"),
        .result      = QDir::toNativeSeparators(result),
        .working_dir = QDir::toNativeSeparators(m_work),
    };

    const auto conflict
        = std::to_array<std::pair<const std::optional<core::rebase::ConflictFile::Entry>&, const QString&>>({
            { file.ancestor, files.base  },
            { file.our,      files.our   },
            { file.their,    files.their },
    });

    for (auto&& [entry, path] : conflict) {
        auto r = writeBlob(m_repo->repo(), entry, path);

        if (!r.has_value()) {
            return std::unexpected<core::Error>(r.error());
        }
    }

    return files;
}

[[nodiscard]] const core::rebase::ConflictFile* ConflictCheckout::find(const ConflictFileId& id) const {
    const auto i = indexOf(id);
    if (!isActive() || i >= m_files.size()) {
        return nullptr;
    }

    return &m_files[i];
}

core::Result<ResolutionState> ConflictCheckout::collect() const {
    if (!isActive()) {
        return std::unexpected<core::Error>({
            .title   = "Could not read the resolutions",
            .message = "No conflict resolution is active",
        });
    }

    if (git_worktree_validate(m_worktree.worktree.get()) != 0) {
        return std::unexpected<core::Error>({
            .title   = "The conflict worktree is no longer valid",
            .message = core::git::get_last_error(),
        });
    }

    if (git_index_read(m_worktree.index.get(), /*force=*/1) != 0) {
        return std::unexpected<core::Error>({
            .title   = "Could not read the worktree index",
            .message = core::git::get_last_error(),
        });
    }

    ResolutionState result;
    result.resolutions.reserve(m_files.size());

    for (std::size_t i = 0; i < m_files.size(); ++i) {
        const auto& file = m_files[i];

        PendingResolution resolution {
            .id       = { .path = file.path, .entry = file.entry },
            .kind     = PendingResolution::Kind::Unchanged,
            .contents = { },
        };

        switch (stateAt(i)) {
        case FileState::Unresolved:
        case FileState::Resolved:
            break;
        case FileState::DeletedPending:
            resolution.kind = PendingResolution::Kind::Deleted;
            break;

        case FileState::ResolvedPending:
        case FileState::ModifiedPending: {
            const git_index_entry* e = git_index_get_bypath(m_worktree.index.get(), file.path.c_str(), 0);

            core::git::blob_t blob;
            if (e == nullptr || git_blob_lookup(&blob, m_worktree.repo.get(), &e->id) != 0) {
                return std::unexpected<core::Error>({
                    .title   = "Could not read the staged file",
                    .message = core::git::get_last_error(),
                });
            }

            resolution.kind     = PendingResolution::Kind::Contents;
            resolution.contents = QByteArray(
                static_cast<const char*>(git_blob_rawcontent(blob.get())),
                static_cast<qsizetype>(git_blob_rawsize(blob.get()))
            );
        }
        }

        result.resolutions.push_back(std::move(resolution));
    }

    auto index_res = copyCurrentIndex();
    if (!index_res.has_value()) {
        return std::unexpected<core::Error>(index_res.error());
    }
    result.index = std::move(index_res).value();

    return result;
}

core::Result<core::git::index_t> ConflictCheckout::copyCurrentIndex() const {
    if (m_worktree.index.get() == nullptr) {
        return std::unexpected<core::Error>({
            .title   = "Could not copy the analysis index",
            .message = "The analysis index is not available",
        });
    }

    core::git::index_t copy_index;
    if (git_index_new(&copy_index) != 0) {
        return std::unexpected<core::Error>({
            .title   = "Could not create the index copy",
            .message = core::git::get_last_error(),
        });
    }

    auto r = copy_index_into(copy_index, m_worktree.index.get());
    if (!r.has_value()) {
        return std::unexpected<core::Error>(r.error());
    }

    return copy_index;
}

QString ConflictCheckout::resultPath(const std::string& relative_path) const {
    return QDir(m_work).filePath(QString::fromUtf8(relative_path.data(), static_cast<qsizetype>(relative_path.size())));
}

[[nodiscard]] std::size_t ConflictCheckout::indexOf(const conflict::ConflictFileId& id) const {
    for (std::size_t i = 0; i < m_files.size(); ++i) {
        const auto& file = m_files[i];
        if (file.path == id.path && file.entry == id.entry) {
            return i;
        }
    }

    return m_files.size();
}
}
