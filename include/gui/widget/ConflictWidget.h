#pragma once

#include "gui/clear_layout.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::widget {

class ConflictWidget : public QWidget {
public:
    ConflictWidget(QWidget* parent = nullptr);

    void addConflictFile(const std::string& path);

    [[nodiscard]] bool isEmpty() const { return m_files->count() == 0; }

    void clearConflicts() { m_files->clear(); }

private:
    QVBoxLayout* m_layout;
    QListWidget* m_files;
};

}
