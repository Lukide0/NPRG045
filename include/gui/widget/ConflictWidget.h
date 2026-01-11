#pragma once

#include "gui/clear_layout.h"
#include <QListWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::widget {

class ConflictWidget : public QWidget {
public:
    ConflictWidget(QWidget* parent = nullptr);

    void addConflictFile(const std::string& path, const std::string& diff_text);

    [[nodiscard]] bool hasConflict() const { return !m_files->isEmpty(); }

    void clearConflicts() { clear_layout(m_files); }

private:
    QVBoxLayout* m_layout;
    QScrollArea* m_scrollarea;
    QVBoxLayout* m_files;
};

}
