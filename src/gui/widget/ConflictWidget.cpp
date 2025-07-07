#include "gui/widget/ConflictWidget.h"

#include <QHBoxLayout>
#include <QWidget>
#include <QListWidget>
#include <QLabel>

namespace gui::widget {

ConflictWidget::ConflictWidget(QWidget* parent)
    : QWidget(parent) {

    m_layout = new QVBoxLayout();
    m_files  = new QListWidget();

    auto* header = new QLabel("Conflicts");
    QFont font   = header->font();
    font.setBold(true);
    font.setPointSize(12);

    header->setFont(font);

    m_layout->addWidget(header);
    m_layout->addWidget(m_files, 1);

    setLayout(m_layout);
}

void ConflictWidget::addConflictFile(const std::string& path) {
    m_files->addItem(QString::fromStdString(path));
}

}
