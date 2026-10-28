#pragma once

#include "conflict/ConflictManager.h"
#include "gui/model/ConflictTreeModel.h"

#include <cstdint>
#include <string>

#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::widget {

class ConflictWidget : public QWidget {
    Q_OBJECT
public:
    using ConflictFile  = model::ConflictTreeModel::ConflictFile;
    using ConflictFiles = QList<ConflictFile>;
    using FileList      = QStringList;

    ConflictWidget(QWidget* parent = nullptr);

    void setConflictFiles(const ConflictFiles& files);

    void setResolvedFiles(const FileList& files);

    void setResolved(const std::string& path, bool resolved = true);
    void setResolved(const QString& path, bool resolved = true);

    [[nodiscard]] bool hasConflict() const;

    void clearConflicts();

signals:
    void resolveRequested(const QString& path, const conflict::ConflictEntry& entry);

private:
    model::ConflictTreeModel* m_model;
    QTreeView* m_view;
    QLabel* m_summary;
    QLabel* m_status_count;
    QStatusBar* m_status_bar;

    std::uint32_t m_conflicts = 0;
    QVBoxLayout* m_layout;
    QScrollArea* m_scrollarea;
    QVBoxLayout* m_files;

    void updateSummary(int unresolved, int total);
    void showSelection(const QModelIndex& current);

    void resolveConflict(const QModelIndex& idx);
};

}
