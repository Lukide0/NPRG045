#pragma once

#include "gui/clear_layout.h"
#include <QVBoxLayout>
#include <QWidget>

namespace gui::widget {

class ConflictWidget : public QWidget {
public:
    ConflictWidget(QWidget* parent = nullptr);

    void AddConflictFile(const std::string& path, const std::string& content);

    void ClearConflicts() { clear_layout(m_layout); }

private:
    QVBoxLayout* m_layout;
};

}
