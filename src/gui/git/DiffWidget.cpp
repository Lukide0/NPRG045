#include "gui/git/DiffWidget.h"

#include "core/action/Action.h"
#include "core/conflict/conflict.h"
#include "core/git/diff.h"
#include "core/git/error.h"
#include "core/git/types.h"
#include "core/patch/PatchSplitter.h"
#include "core/patch/split.h"
#include "gui/clear_layout.h"
#include "gui/error.h"
#include "gui/git/DiffEditor.h"
#include "gui/git/DiffEditorLine.h"
#include "gui/git/DiffFile.h"
#include "gui/style/DiffStyle.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include <git2/commit.h>
#include <git2/diff.h>
#include <git2/errors.h>
#include <git2/patch.h>
#include <git2/types.h>

#include <QFont>
#include <QFrame>
#include <QList>
#include <QMenu>
#include <QScrollArea>
#include <QScrollBar>
#include <QString>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::git {

using core::git::diff_files_t;
using core::git::diff_hunk_t;
using core::git::diff_line_t;
using core::git::diff_result_t;

using core::action::Action;

DiffWidget::DiffWidget(QWidget* parent)
    : QWidget(parent) {

    m_scrollarea = new QScrollArea(this);
    m_scrollarea->setWidgetResizable(true);

    m_scroll_content = new QWidget();

    m_scroll_layout = new QVBoxLayout(m_scroll_content);
    m_scroll_layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    m_scroll_layout->setContentsMargins(0, 0, 0, 0);

    m_scrollarea->setWidget(m_scroll_content);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_scrollarea);
    setLayout(m_layout);
}

void DiffWidget::setSession(core::rebase::RebaseSession* session, app::rebase::RebaseController* controller) {
    m_controller = controller;
    m_rebase     = session;
}

void DiffWidget::ensureEditorVisible(DiffFile* file) {
    if (file == nullptr) {
        return;
    }

    auto* bar = m_scrollarea->verticalScrollBar();
    bar->setValue(file->y());
}

void DiffWidget::clear() {
    clear_layout(m_scroll_layout);
    m_files.clear();
    m_diffs.clear();
}

void DiffWidget::setCurrentStep(core::rebase::RebaseAnalysisConstPtr analysis, int row) {
    using core::conflict::ConflictState;

    clear();

    if (m_rebase == nullptr) {
        return;
    }

    m_analysis = analysis;

    // invalid row
    if (analysis == nullptr || row < 0 || static_cast<std::size_t>(row) >= analysis->steps.size()) {
        return;
    }

    const auto& step = analysis->steps[row];
    // conflict or unknown
    if (!step.has_parent_tree || !step.has_result()) {
        return;
    }

    m_step = &step;

    if (row - 1 >= 0) {
        m_prev_step = &m_analysis->steps[row - 1];
    }

    diff_result_t res = core::git::prepare_resolution_diff(&step.parent_tree_id, step, m_rebase->conflicts());
    update(res, true);
}

void DiffWidget::setCurrentCommit(git_commit* commit) {
    clear();

    if (commit == nullptr || m_rebase == nullptr) {
        return;
    }

    diff_result_t res;
    core::git::commit_t parent_commit;
    std::uint32_t parents = git_commit_parentcount(commit);

    switch (parents) {
    case 0:
        res = core::git::prepare_diff(nullptr, commit);
        break;
    case 1:
        if (git_commit_parent(&parent_commit, commit, 0) != 0) {
            res.state = diff_result_t::FAILED_TO_CREATE_DIFF;
        } else {
            res = core::git::prepare_diff(parent_commit, commit);
        }
        break;
    default:
        res.state = diff_result_t::FAILED_TO_CREATE_DIFF;
        break;
    }

    update(res, false);
}

void DiffWidget::update(core::git::diff_result_t& res, bool editable) {
    switch (res.state) {
    case diff_result_t::FAILED_TO_RETRIEVE_TREE:
        DISPLAY_ERROR(this, "Failed to retrieve tree from commit", core::git::get_last_error());
        return;

    case diff_result_t::FAILED_TO_CREATE_DIFF:
        DISPLAY_ERROR(this, "Failed to create diff", core::git::get_last_error());
        return;
    case diff_result_t::OK:
        break;
    }

    m_diffs = core::git::create_diff(res.diff);
    for (std::size_t i = 0; i < m_diffs.size(); ++i) {
        if (i != 0) {
            auto* line = new QFrame(this);
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Sunken);
            line->setLineWidth(1);
            m_scroll_layout->addWidget(line);
        }

        const auto& file_diff = m_diffs[i];
        createFileDiff(file_diff, editable);
    }

    for (auto* file : m_files) {
        file->updateEditorSize();
    }

    m_scroll_layout->addStretch(1);
}

QString create_diff_header(const diff_files_t& diff) {
    QString header;
    switch (diff.state) {
    case diff_files_t::State::ADDED:
        header += "New: ";
        header += QString::fromStdString(diff.new_file.path);
        break;
    case diff_files_t::State::DELETED:
        header += "Deleted: ";
        header += QString::fromStdString(diff.old_file.path);
        break;
    case diff_files_t::State::MODIFIED:
        header += "Modified: ";
        header += QString::fromStdString(diff.new_file.path);
        break;
    case diff_files_t::State::MOVED:
        header += "Moved: ";
        header += QString::fromStdString(diff.old_file.path);
        header += " -> ";
        header += QString::fromStdString(diff.new_file.path);
        break;
    case diff_files_t::State::COPIED:
        header += "Copied: ";
        header += QString::fromStdString(diff.old_file.path);
        header += " -> ";
        header += QString::fromStdString(diff.new_file.path);
        break;
    default:
        return "";
    }

    return header;
}

void DiffWidget::createFileDiff(const diff_files_t& diff, bool editable) {
    auto* file_diff = new DiffFile(diff);
    m_curr_editor   = file_diff->getEditor();
    m_curr_editor->enableContextMenu(editable);

    QString header = create_diff_header(diff);
    if (header.isEmpty()) {
        const std::string& path = (!diff.old_file.path.empty()) ? diff.old_file.path : diff.new_file.path;

        DISPLAY_ERROR(
            this,
            "Repository error",
            std::format("File '{}' has unsupported state '{}'", path, diff.state_to_str(diff.state))
        );
        return;
    }

    file_diff->setDiff(core::git::diff_header(diff));
    file_diff->setHeader(header);

    std::vector<section_t> sections;
    for (const auto& hunk : diff.hunks) {
        addHunkDiff(hunk, sections);
    }

    QList<QTextEdit::ExtraSelection> text_sections;
    for (auto&& section : sections) {

        QTextEdit::ExtraSelection text_section;
        text_section.cursor = QTextCursor(section.block);
        text_section.cursor.clearSelection();
        text_section.format.setForeground(style::DiffStyle::getColor(section.type));

        if (section.type == section_t::Type::INFO) {
            text_section.format.setFontWeight(QFont::Bold);
        }

        text_section.format.setProperty(QTextFormat::FullWidthSelection, true);
        text_sections.append(text_section);
    }

    m_curr_editor->setExtraSelections(text_sections);
    m_scroll_layout->addWidget(file_diff);
    m_files.push_back(file_diff);
    m_curr_editor->enableContextMenu(editable);

    connect(m_curr_editor, &DiffEditor::extendContextMenu, this, [this](QMenu* menu) {
        menu->addSeparator();
        auto* split_act = menu->addAction("Split commit");

        connect(split_act, &QAction::triggered, this, &DiffWidget::splitCommitEvent);
    });
}

void DiffWidget::addHunkDiff(const diff_hunk_t& hunk, std::vector<section_t>& sections) {

    m_curr_editor->setUpdatesEnabled(false);

    QString hunk_info = QString::fromStdString(
        std::format(
            "@@ -{},{} +{},{} @@ {}",
            hunk.old_file.offset,
            hunk.old_file.count,
            hunk.new_file.offset,
            hunk.new_file.count,
            hunk.header_context
        )
    );
    auto* document = m_curr_editor->document();
    QTextCursor cursor(document);

    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::MoveOperation::End);
    cursor.insertText(hunk_info);

    sections.emplace_back(section_t::Type::INFO, cursor.block());

    cursor.insertText("\n");

    for (const auto& line : hunk.lines) {
        addLineDiff(cursor, hunk, line, sections);
    }

    cursor.endEditBlock();
    m_curr_editor->setUpdatesEnabled(true);
    m_curr_editor->viewport()->update();
}

void DiffWidget::addLineDiff(
    QTextCursor& cursor, const diff_hunk_t& hunk, const diff_line_t& line, std::vector<section_t>& sections
) {
    QString new_content = QString::fromStdString(line.content);
    new_content.prepend(' ');

    cursor.insertText(new_content);
    QTextBlock block = cursor.block();
    block.setUserData(new DiffEditorLineData(line, hunk));
    cursor.insertText("\n");

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

    sections.emplace_back(type, block);
}

void DiffWidget::splitCommitEvent() {
    using FileState = core::patch::PatchSplitter::FileState;

    std::string patch_text;

    {
        core::patch::PatchSplitter splitter;

        for (auto&& file : m_files) {
            const DiffEditor* editor = file->getEditor();

            if (!editor->selectedLineOrFile()) {
                splitter.not_selected_file();
                continue;
            }

            FileState state = FileState::ALL_SELECTED;
            if (!splitter.file_begin(file->getDiff(), state)) {
                DISPLAY_ERROR(
                    this,
                    "Unsupported  file state",
                    std::format("File state '{}' is not supported", diff_files_t::state_to_str(file->getDiff().state))
                );
                return;
            }

            std::vector<diff_line_t> lines;

            editor->processLines([&](const DiffEditorLineData& line) {
                splitter.process(line.get_line(), line.get_hunk(), line.is_selected());
            });

            splitter.file_end();
        }

        if (splitter.is_whole_patch()) {
            DISPLAY_ERROR(
                this, "Invalid split", "The split must contain a non-empty subset of the patch, not entire patch"
            );
            return;
        }

        patch_text = splitter.get_patch();
    }

    core::git::diff_t diff;

    int state = git_diff_from_buffer(&diff, patch_text.c_str(), patch_text.size());
    if (state != 0) {
        DISPLAY_ERROR(this, "Failed to create diff", core::git::get_last_error());
        return;
    }

    core::git::commit_t first_commit;
    core::git::commit_t second_commit;

    core::patch::SplitError err
        = core::patch::split(*m_rebase, first_commit, second_commit, *m_step, m_prev_step, diff);
    if (err.has_error()) {
        DISPLAY_ERROR(this, "Failed to create split", std::format("{}: {}.", err.title, err.message));
        return;
    }

    m_controller->splitAction(static_cast<int>(m_step->index), std::move(first_commit), std::move(second_commit));
}

}
