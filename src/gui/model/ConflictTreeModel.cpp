#include "gui/model/ConflictTreeModel.h"
#include "core/conflict/ConflictManager.h"

#include <QAbstractItemModel>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <Qt>
#include <QVariant>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

namespace gui::model {

ConflictTreeModel::ConflictTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
    , m_root(std::make_unique<Node>()) { }

bool ConflictTreeModel::addPath(const QString& fullPath, const core::conflict::ConflictEntry& entry) {
    Chain chain = makeChain(fullPath);
    if (chain.top == nullptr) {
        return false;
    }

    chain.leaf->conflict_entry = entry;

    // insert at the sorted position, announcing only the top of the new chain
    auto& siblings = chain.parent->children;
    auto it = std::ranges::lower_bound(siblings, chain.top, [](const auto& a, const auto& b) { return less(*a, *b); });
    const int row = static_cast<int>(it - siblings.begin());

    beginInsertRows(indexOf(chain.parent), row, row);
    {
        siblings.insert(it, std::move(chain.top));
        m_files.insert(fullPath, chain.leaf);

        adjustUnresolvedCount(chain.leaf, 1);
    }
    endInsertRows();

    refreshAncestors(chain.parent);
    emitCounts();

    return true;
}

void ConflictTreeModel::setPaths(const QList<ConflictFile>& files) {
    beginResetModel();

    m_root = std::make_unique<Node>();
    m_files.clear();

    for (const auto& file : files) {
        Chain chain = makeChain(file.path);
        if (chain.top == nullptr) {
            continue;
        }

        chain.leaf->conflict_entry = file.entry;
        chain.parent->children.push_back(std::move(chain.top));

        m_files.insert(file.path, chain.leaf);
        adjustUnresolvedCount(chain.leaf, 1);
    }
    sortChildren(*m_root);

    endResetModel();
    emitCounts();
}

void ConflictTreeModel::clear() {
    beginResetModel();

    m_root = std::make_unique<Node>();
    m_files.clear();

    endResetModel();
    emitCounts();
}

void ConflictTreeModel::setResolved(const QString& path, bool resolved) {
    Node* node = m_files.value(path, nullptr);

    if (node == nullptr || node->resolved == resolved) {
        return;
    }

    node->resolved = resolved;

    adjustUnresolvedCount(node, resolved ? -1 : +1);

    refreshAncestors(node);
    emitCounts();
}

void ConflictTreeModel::setResolvedFiles(const QStringList& paths) {
    QSet<Node*> dirty;
    bool changed = false;

    for (const QString& path : paths) {
        Node* node = m_files.value(path, nullptr);

        if (node == nullptr || node->resolved) {
            continue;
        }

        node->resolved = true;
        adjustUnresolvedCount(node, -1);

        markAncestorsDirty(node, dirty);
        changed = true;
    }

    if (!changed) {
        return;
    }

    for (Node* node : dirty) {
        notifyNodeChanged(node);
    }

    emitCounts();
}

[[nodiscard]] QModelIndex ConflictTreeModel::indexForPath(const QString& path) const {
    Node* node = m_files.value(path, nullptr);
    return node != nullptr ? indexOf(node) : QModelIndex { };
}

[[nodiscard]] QString ConflictTreeModel::pathAt(const QModelIndex& idx) const {
    if (!idx.isValid() || idx.model() != this) {
        return { };
    }

    return idx.siblingAtColumn(NameColumn).data(PathRole).toString();
}

[[nodiscard]] std::optional<core::conflict::ConflictEntry> ConflictTreeModel::entryAt(const QModelIndex& idx) const {

    if (!idx.isValid() || idx.model() != this) {
        return std::nullopt;
    }

    const Node* node = nodeFor(idx);

    if (!node->is_file) {
        return std::nullopt;
    }

    return node->conflict_entry;
}

[[nodiscard]] QModelIndex ConflictTreeModel::index(int row, int column, const QModelIndex& parent) const {

    if (row < 0 || column < 0 || column >= ColumnCount
        || (parent.isValid() && (parent.model() != this || parent.column() != NameColumn))) {
        return { };
    }

    const Node* parentNode = nodeFor(parent);

    if (row >= static_cast<int>(parentNode->children.size())) {
        return { };
    }

    Node* child = parentNode->children[static_cast<std::size_t>(row)].get();

    return createIndex(row, column, child);
}

[[nodiscard]] QModelIndex ConflictTreeModel::parent(const QModelIndex& child) const {

    if (!child.isValid() || child.model() != this) {
        return { };
    }

    Node* parentNode = nodeFor(child)->parent;

    if (parentNode == nullptr || parentNode == m_root.get()) {
        return { };
    }

    const int row = rowOf(parentNode);

    return row >= 0 ? createIndex(row, NameColumn, parentNode) : QModelIndex { };
}

[[nodiscard]] int ConflictTreeModel::rowCount(const QModelIndex& parent) const {

    if (parent.isValid() && (parent.model() != this || parent.column() != NameColumn)) {
        return 0;
    }

    return static_cast<int>(nodeFor(parent)->children.size());
}

[[nodiscard]] int ConflictTreeModel::columnCount(const QModelIndex& /*parent*/) const { return ColumnCount; }

[[nodiscard]] QVariant ConflictTreeModel::data(const QModelIndex& idx, int role) const {

    if (!idx.isValid() || idx.model() != this) {
        return { };
    }

    const Node* node = nodeFor(idx);

    switch (role) {
    case PathRole:
        return node->is_file ? QVariant(node->path) : QVariant { };

    case IsFileRole:
        return node->is_file;

    case ResolvedRole:
        return node->unresolved_count == 0;

    case Qt::ToolTipRole:
        return node->is_file ? QVariant(node->path) : QVariant { };

    case Qt::DisplayRole:
        break;

    default:
        return { };
    }

    if (idx.column() == NameColumn) {
        return node->name;
    }

    if (node->is_file) {
        return node->resolved ? QStringLiteral("Resolved") : QStringLiteral("Unresolved");
    }

    const int unresolved = node->unresolved_count;

    return unresolved == 0 ? QStringLiteral("Resolved") : QStringLiteral("%1 unresolved").arg(unresolved);
}

[[nodiscard]] QVariant ConflictTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {

    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return { };
    }

    switch (section) {
    case NameColumn:
        return QStringLiteral("File");

    case StatusColumn:
        return QStringLiteral("Status");

    default:
        return { };
    }
}

[[nodiscard]] Qt::ItemFlags ConflictTreeModel::flags(const QModelIndex& idx) const {

    if (!idx.isValid() || idx.model() != this) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool ConflictTreeModel::less(const Node& a, const Node& b) {
    // Directories first.
    if (a.is_file != b.is_file) {
        return !a.is_file;
    }

    // Case-insensitive display order, with a deterministic tie-breaker.
    const int insensitive = QString::compare(a.name, b.name, Qt::CaseInsensitive);

    if (insensitive != 0) {
        return insensitive < 0;
    }

    return QString::compare(a.name, b.name, Qt::CaseSensitive) < 0;
}

QStringList ConflictTreeModel::splitValidPath(const QString& path) {
    if (path.isEmpty() || path.startsWith('/') || path.endsWith('/')) {
        return { };
    }

    const QStringList parts = path.split('/', Qt::KeepEmptyParts);

    for (const QString& part : parts) {
        if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral("..")) {
            return { };
        }
    }

    return parts;
}

ConflictTreeModel::Chain ConflictTreeModel::makeChain(const QString& fullPath) {
    if (m_files.contains(fullPath)) {
        return { };
    }

    const QStringList parts = splitValidPath(fullPath);

    if (parts.isEmpty()) {
        return { };
    }

    Node* current  = m_root.get();
    qsizetype from = 0;

    for (; from < parts.size() - 1; ++from) {
        const QString& component = parts[from];

        Node* directory = findChild(*current, component, false);

        if (directory != nullptr) {
            current = directory;
            continue;
        }

        // A file cannot also be the parent directory of another path.
        if (findChild(*current, component, true) != nullptr) {
            return { };
        }

        break;
    }

    const QString& filename = parts.back();

    // Prevent a file from being inserted alongside a same-named directory.
    if (findChild(*current, filename, false) != nullptr || findChild(*current, filename, true) != nullptr) {
        return { };
    }

    Chain chain;
    chain.parent = current;

    Node* tail = nullptr;

    for (qsizetype i = from; i < parts.size(); ++i) {
        auto node = std::make_unique<Node>();

        node->name    = parts[i];
        node->is_file = (i == parts.size() - 1);

        Node* raw = node.get();

        if (!chain.top) {
            chain.top = std::move(node);
        } else {
            raw->parent = tail;
            tail->children.push_back(std::move(node));
        }

        tail = raw;
    }

    tail->path        = fullPath;
    chain.top->parent = current;
    chain.leaf        = tail;

    return chain;
}

void ConflictTreeModel::sortChildren(Node& node) {
    std::ranges::sort(node.children, [](const auto& a, const auto& b) { return less(*a, *b); });

    for (auto& child : node.children) {
        sortChildren(*child);
    }
}

ConflictTreeModel::Node* ConflictTreeModel::findChild(const Node& parent, const QString& name, bool isFile) {

    for (const auto& child : parent.children) {
        if (child->is_file == isFile && child->name == name) {
            return child.get();
        }
    }

    return nullptr;
}

[[nodiscard]] ConflictTreeModel::Node* ConflictTreeModel::nodeFor(const QModelIndex& idx) const {
    return idx.isValid() ? static_cast<Node*>(idx.internalPointer()) : m_root.get();
}

[[nodiscard]] int ConflictTreeModel::rowOf(const Node* node) const {
    if (node == nullptr || node->parent == nullptr) {
        return -1;
    }

    const Node* parentNode = node->parent;

    for (std::size_t i = 0; i < parentNode->children.size(); ++i) {
        if (parentNode->children[i].get() == node) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

[[nodiscard]] QModelIndex ConflictTreeModel::indexOf(Node* node) const {
    if (node == nullptr || node == m_root.get()) {
        return { };
    }

    const int row = rowOf(node);

    return row >= 0 ? createIndex(row, NameColumn, node) : QModelIndex { };
}

void ConflictTreeModel::adjustUnresolvedCount(Node* node, int delta) {
    for (; node != nullptr; node = node->parent) {
        node->unresolved_count += delta;
    }
}

void ConflictTreeModel::notifyNodeChanged(Node* node) {
    if (node == nullptr || node == m_root.get()) {
        return;
    }

    const int row = rowOf(node);

    if (row < 0) {
        return;
    }

    const QModelIndex first = createIndex(row, NameColumn, node);
    const QModelIndex last  = createIndex(row, StatusColumn, node);

    emit dataChanged(first, last, { Qt::DisplayRole, ResolvedRole });
}

void ConflictTreeModel::refreshAncestors(Node* node) {
    for (; node != nullptr && node != m_root.get(); node = node->parent) {
        notifyNodeChanged(node);
    }
}

void ConflictTreeModel::markAncestorsDirty(Node* node, QSet<Node*>& dirty) {
    for (; node != nullptr; node = node->parent) {
        dirty.insert(node);
    }
}

void ConflictTreeModel::emitCounts() { emit countsChanged(unresolvedCount(), totalCount()); }

}
