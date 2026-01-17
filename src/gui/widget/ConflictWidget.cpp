#include "gui/widget/ConflictWidget.h"
#include "gui/widget/ConflictFile.h"
#include "gui/widget/ConflictHighlighter.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QWidget>

namespace gui::widget {

ConflictWidget::ConflictWidget(QWidget* parent)
    : QWidget(parent) {

    m_scrollarea = new QScrollArea(this);
    m_scrollarea->setWidgetResizable(true);

    auto* scroll_content = new QWidget();

    m_files = new QVBoxLayout(scroll_content);
    m_files->setSizeConstraint(QLayout::SetMinAndMaxSize);
    m_files->setContentsMargins(0, 0, 0, 0);

    m_scrollarea->setWidget(scroll_content);

    auto* header = new QLabel("Conflicts");

    QFont font = header->font();
    font.setBold(true);
    font.setPointSize(12);

    header->setFont(font);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_layout->addWidget(header);
    m_layout->addWidget(m_scrollarea);
    setLayout(m_layout);
}

void ConflictWidget::addConflictFile(const std::string& path, const std::string& diff_text) {
    auto* file = new ConflictFile(QString::fromStdString(path));

    file->setContent(QString::fromStdString(diff_text));

    if (hasConflict()) {
        auto* line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        line->setLineWidth(1);
        line->setMidLineWidth(0);
        m_files->addWidget(line);
    }

    auto* _ = new ConflictHighlighter(file->getDocument());

    file->updateEditorHeight();

    m_files->addWidget(file);
}

void ConflictWidget::addConflictFile(const std::string& path, const core::git::conflict_diff_t& diff) {
    addConflictFile(path, diff.diff);
}

}
