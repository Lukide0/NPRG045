#pragma once
#include "core/conflict/ConflictManager.h"
#include <QAbstractItemModel>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <Qt>
#include <QtTypes>

#include <memory>
#include <vector>

namespace app::conflict {

/**
 * @brief Tree of conflicted files grouped by directory.
 *
 * Paths are repo-relative with '/' separators ("src/ui/a.cpp"). Files are added
 * incrementally, so no model reset is needed while conflicts are being collected.
 */
class ConflictTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Column {
        NameColumn = 0,
        StatusColumn,
        ColumnCount,
    };

    enum Role {
        PathRole = Qt::UserRole + 1, // full path (files only, empty for directories)
        IsFileRole, // bool
        ResolvedRole, // bool
    };

    struct ConflictFile {
        std::size_t id;
        QString path;
        core::conflict::ConflictEntry entry;
        bool resolved;
    };

    explicit ConflictTreeModel(QObject* parent = nullptr);

    /// Replaces the whole content in one step: a single modelReset and a single countsChanged.
    void setPaths(const QList<ConflictFile>& files);

    void clear();

    void setResolved(const QString& path, bool resolved);

    /// Marks a file resolved/unresolved; refreshes the file row and every ancestor's counter.
    void setResolvedFiles(const QStringList& paths);

    [[nodiscard]] int unresolvedCount() const { return m_root->unresolved_count; }

    [[nodiscard]] int totalCount() const { return static_cast<int>(m_files.size()); }

    [[nodiscard]] QModelIndex indexForPath(const QString& path) const;

    /**
     * Returns the path of a file, or an empty string for a directory
     * or invalid index.
     */
    [[nodiscard]] QString pathAt(const QModelIndex& idx) const;

    /**
     * Returns the ConflictEntry associated with a file.
     * Directory nodes and invalid indexes return std::nullopt.
     */
    [[nodiscard]] std::optional<core::conflict::ConflictEntry> entryAt(const QModelIndex& idx) const;

    [[nodiscard]] std::size_t idAt(const QModelIndex& idx) const;

    [[nodiscard]] bool isResolved(const QModelIndex& idx) const;

    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent = { }) const override;

    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;

    [[nodiscard]] int rowCount(const QModelIndex& parent = { }) const override;

    [[nodiscard]] int columnCount(const QModelIndex& /*parent*/ = { }) const override;

    [[nodiscard]] QVariant data(const QModelIndex& idx, int role) const override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& idx) const override;

signals:
    void countsChanged(int unresolved, int total);

private:
    struct Node {
        QString name;
        QString path;

        bool is_file  = false;
        bool resolved = false;

        Node* parent = nullptr;

        // Present for file nodes that represent conflicts.
        std::optional<core::conflict::ConflictEntry> conflict_entry;
        std::size_t id = 0;

        // Cached count of unresolved files in this subtree.
        int unresolved_count = 0;

        std::vector<std::unique_ptr<Node>> children;
    };

    std::unique_ptr<Node> m_root;
    QHash<QString, Node*> m_files;

    struct Chain {
        Node* parent = nullptr;
        std::unique_ptr<Node> top;
        Node* leaf = nullptr;

        explicit operator bool() const noexcept { return top != nullptr && parent != nullptr && leaf != nullptr; }
    };

    /**
     * Finds an existing directory prefix and constructs the missing
     * chain without inserting anything into the model.
     */
    Chain makeChain(const QString& fullPath);

    [[nodiscard]] Node* nodeFor(const QModelIndex& idx) const;

    [[nodiscard]] int rowOf(const Node* node) const;

    [[nodiscard]] QModelIndex indexOf(Node* node) const;

    /**
     * Applies a count delta from a node up to the root.
     *
     * A newly added unresolved file adds 1; resolving it subtracts 1.
     */
    void adjustUnresolvedCount(Node* node, int delta);

    void notifyNodeChanged(Node* node);

    // Emits updates for a file and all directory ancestors.
    void refreshAncestors(Node* node);

    void emitCounts();

    /**
     * Accepts repository-relative paths using '/' separators.
     * Rejects absolute paths, empty components, "." and "..".
     */
    static QStringList splitValidPath(const QString& path);
    static bool less(const Node& a, const Node& b);
    static void sortChildren(Node& node);
    static Node* findChild(const Node& parent, const QString& name, bool isFile);
    static void markAncestorsDirty(Node* node, QSet<Node*>& dirty);
};

}
