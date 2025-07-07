#pragma once

#include "gui/clear_layout.h"
#include <QVBoxLayout>
#include <QWidget>
#include <QListWidget>

namespace gui::widget {

class ConflictWidget : public QWidget {
public:
    ConflictWidget(QWidget* parent = nullptr);

    void addConflictFile(const std::string& path);

    void clearConflicts() { m_files->clear(); }

private:
    QVBoxLayout* m_layout;
    QListWidget* m_files;
};

}
