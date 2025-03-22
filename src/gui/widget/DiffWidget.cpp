#include "gui/widget/DiffWidget.h"
#include "core/git/diff.h"
#include "core/utils/todo.h"
#include "gui/clear_layout.h"
#include "gui/widget/DiffEditor.h"
#include "gui/widget/DiffEditorLine.h"
#include "gui/widget/DiffFile.h"
#include "gui/widget/graph/Node.h"

#include <format>
#include <git2/diff.h>
#include <git2/types.h>
#include <QMessageBox>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <vector>

DiffWidget::DiffWidget(QWidget* parent)
    : QWidget(parent) {

    m_scrollarea = new QScrollArea(this);
    m_scrollarea->setWidgetResizable(true);

    m_scroll_content = new QWidget();

    m_scroll_layout = new QVBoxLayout(m_scroll_content);
    m_scroll_layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    m_scroll_layout->setContentsMargins(0, 0, 0, 0);
    m_scroll_layout->addStretch();

    m_scrollarea->setWidget(m_scroll_content);

    m_layout = new QVBoxLayout(this);
    m_layout->addWidget(m_scrollarea);
    setLayout(m_layout);
}

void DiffWidget::update(Node* node) {
    m_node = node;
    clear_layout(m_scroll_layout);

    if (m_node == nullptr) {
        return;
    }

    git_commit* commit        = node->getCommit();
    git_commit* parent_commit = nullptr;

    if (Node* parent_node = node->getParentNode()) {
        parent_commit = parent_node->getCommit();
    }

    diff_result_t res = prepare_diff(parent_commit, commit);
    switch (res.state) {
    case diff_result_t::FAILED_TO_RETRIEVE_TREE:
        QMessageBox::critical(this, "Commit diff error", "Failed to retrieve tree from commit");
        return;

    case diff_result_t::FAILED_TO_CREATE_DIFF:
        QMessageBox::critical(this, "Commit diff error", "Failed to create diff");
        return;
    case diff_result_t::OK:
        break;
    }

    m_diffs = create_diff(res.diff.diff);

    for (std::size_t i = 0; i < m_diffs.size(); ++i) {
        if (i != 0) {
            auto* line = new QFrame(this);
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            line->setLineWidth(1);
            m_scroll_layout->addWidget(line);
        }

        const auto& file_diff = m_diffs[i];
        createFileDiff(file_diff);
    }
}

void DiffWidget::createFileDiff(const diff_files_t& diff) {
    auto* file_diff = new DiffFile();
    m_curr_editor   = file_diff->getEditor();

    QString header;
    switch (diff.state) {
    case diff_files_t::State::ADDED:
        header += "New: ";
        header += diff.new_file.path;
        break;
    case diff_files_t::State::DELETED:
        header += "Deleted: ";
        header += diff.old_file.path;
        break;
    case diff_files_t::State::MODIFIED:
        header += "Modified: ";
        header += diff.new_file.path;
        break;
    case diff_files_t::State::RENAMED:
        header += "Moved: ";
        header += diff.old_file.path;
        header += " -> ";
        header += diff.new_file.path;
        break;
    case diff_files_t::State::COPIED:
        header += "Copied: ";
        header += diff.old_file.path;
        header += " -> ";
        header += diff.new_file.path;
        break;
    default:
        TODO("Unsupported file state");
        break;
    }

    file_diff->setHeader(header);

    std::vector<section_t> sections;

    for (const auto& hunk : diff.hunks) {
        addHunkDiff(hunk, sections);
    }

    QList<QTextEdit::ExtraSelection> text_sections;
    for (auto&& section : sections) {

        QTextCursor cursor(m_curr_editor->document());
        cursor.setPosition(section.start);
        cursor.setPosition(section.end, QTextCursor::KeepAnchor);

        QTextEdit::ExtraSelection text_section;
        text_section.cursor = cursor;
        text_section.format.setForeground(DiffEditorLine::ConvertToColor(section.type));

        if (section.type == section_t::Type::HUNK_INFO) {
            text_section.format.setFontWeight(QFont::Bold);
        }

        text_sections.append(text_section);
    }

    file_diff->updateEditorHeight();

    m_curr_editor->setExtraSelections(text_sections);
    m_scroll_layout->addWidget(file_diff);
}

void DiffWidget::addHunkDiff(const diff_hunk_t& hunk, std::vector<section_t>& sections) {

    QString hunk_info;
    hunk_info += std::format(
        "@@ -{},{} +{},{} @@ {}",
        hunk.old_file.offset,
        hunk.old_file.count,
        hunk.new_file.offset,
        hunk.new_file.count,
        hunk.header_context
    );

    m_curr_editor->appendPlainText(hunk_info);

    auto* document = m_curr_editor->document();
    QTextCursor cursor(document);
    cursor.movePosition(QTextCursor::MoveOperation::End);

    QTextBlock block = cursor.block();
    section_t section;
    section.type  = section_t::Type::HUNK_INFO;
    section.start = block.position();
    section.end   = section.start + block.length() - 1;

    sections.push_back(section);

    for (const auto& line : hunk.lines) {
        addLineDiff(hunk, line, sections);
    }
}

void DiffWidget::addLineDiff(const diff_hunk_t& hunk, const diff_line_t& line, std::vector<section_t>& sections) {

    m_curr_editor->appendPlainText(line.content.c_str());

    auto* document = m_curr_editor->document();

    QTextCursor cursor(document);
    cursor.movePosition(QTextCursor::MoveOperation::End);

    QTextBlock block = cursor.block();
    block.setUserData(new DiffEditorLineData(&line, &hunk));

    section_t::Type type;
    switch (line.type) {
    case diff_line_t::Type::CONTEXT:
    case diff_line_t::Type::CONTEXT_NO_NEWLINE:
        return;
    case diff_line_t::Type::ADDITION:
    case diff_line_t::Type::ADDITION_NEWLINE:
        type = section_t::Type::ADDITION;
        break;
    case diff_line_t::Type::DELETION:
    case diff_line_t::Type::DELETION_NEWLINE:
        type = section_t::Type::DELETION;
        break;
    }

    section_t section;
    section.type  = type;
    section.start = block.position();
    section.end   = section.start + block.length() - 1;

    sections.push_back(section);
}
