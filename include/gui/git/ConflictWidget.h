#pragma once

#include "app/conflict/ConflictEditorSession.h"
#include "app/conflict/ConflictTreeModel.h"
#include "core/conflict/ConflictManager.h"

#include <cstdint>
#include <string>

#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::git {

class ConflictWidget : public QWidget {
    Q_OBJECT
public:
    using ConflictFile  = app::conflict::ConflictTreeModel::ConflictFile;
    using ConflictFiles = QList<ConflictFile>;
    using FileList      = QStringList;

    enum class ConflictEditMode {
        Resolve,
        EditResolution,
    };

    ConflictWidget(QWidget* parent = nullptr);

    void setConflictFiles(const ConflictFiles& files);

    void setResolvedFiles(const FileList& files);

    void setResolved(const std::string& path, bool resolved = true);
    void setResolved(const QString& path, bool resolved = true);

    [[nodiscard]] bool hasConflict() const;

    void clearConflicts();

signals:
    void resolveRequested(
        std::size_t id, const QString& path, const core::conflict::ConflictEntry& entry, ConflictEditMode mode
    );

private:
    app::conflict::ConflictTreeModel* m_model;
    QTreeView* m_view;
    QLabel* m_summary;
    QLabel* m_status_count;
    QStatusBar* m_status_bar;

    struct {
        QAction* resolve_conflict;
        QAction* edit_resolution;
        QToolButton* widget;

    } m_button;

    std::uint32_t m_conflicts = 0;
    QVBoxLayout* m_layout;
    QScrollArea* m_scrollarea;
    QVBoxLayout* m_files;

    void updateSummary(int unresolved, int total);
    void showSelection(const QModelIndex& current);

    void resolveConflict(ConflictEditMode mode);
};

}
