#include "gui/widget/ConflictWidget.h"
#include "gui/widget/ConflictFile.h"

#include <QHBoxLayout>
#include <QWidget>

namespace gui::widget {

ConflictWidget::ConflictWidget(QWidget* parent)
    : QWidget(parent) {

    m_layout = new QVBoxLayout();
    m_files  = new QVBoxLayout();

    auto* header = new QLabel("Conflicts");
    QFont font   = header->font();
    font.setBold(true);
    font.setPointSize(12);

    header->setFont(font);

    m_layout->addWidget(header);
    m_layout->addLayout(m_files, 1);

    setLayout(m_layout);
}

void ConflictWidget::addConflictFile(const std::string& path, const std::string& content) {
    auto* file   = new ConflictFile();
    auto* editor = file->getEditor();

    file->setHeader(QString::fromStdString(path));

    editor->setPlainText(QString::fromStdString(content));
    editor->setReadOnly(true);

    file->updateEditorHeight();

    m_files->addWidget(file);
}

}
