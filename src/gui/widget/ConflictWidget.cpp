#include "gui/widget/ConflictWidget.h"
#include "gui/widget/ConflictFile.h"

#include <QHBoxLayout>
#include <QWidget>

namespace gui::widget {

ConflictWidget::ConflictWidget(QWidget* parent)
    : QWidget(parent) {

    m_layout = new QVBoxLayout();

    setLayout(m_layout);
}

void ConflictWidget::addConflictFile(const std::string& path, const std::string& content) {
    auto* file   = new ConflictFile();
    auto* editor = file->getEditor();

    file->setHeader(QString::fromStdString(path));

    editor->setPlainText(QString::fromStdString(content));
    editor->setReadOnly(true);

    m_layout->addWidget(file);
}

}
