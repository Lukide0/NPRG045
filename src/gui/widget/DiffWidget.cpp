#include "gui/widget/DiffWidget.h"
#include "action/Action.h"
#include "action/ActionManager.h"
#include "App.h"
#include "core/git/diff.h"
#include "core/git/types.h"
#include "core/patch/PatchSplitter.h"
#include "core/patch/split.h"
#include "core/state/CommandHistory.h"
#include "gui/clear_layout.h"
#include "gui/widget/DiffEditor.h"
#include "gui/widget/DiffEditorLine.h"
#include "gui/widget/DiffFile.h"
#include "logging/Log.h"

#include <cstddef>
#include <format>
#include <memory>
#include <utility>
#include <vector>

#include <git2/diff.h>
#include <git2/errors.h>
#include <git2/patch.h>
#include <git2/types.h>

#include <QFont>
#include <QFrame>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QString>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::widget {

using core::git::diff_files_t;
using core::git::diff_hunk_t;
using core::git::diff_line_t;
using core::git::diff_result_t;

using action::Action;

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
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_scrollarea);
    setLayout(m_layout);
}

void DiffWidget::ensureEditorVisible(DiffFile* file) {
    auto* bar = m_scrollarea->verticalScrollBar();
    bar->setValue(file->y());
}

void DiffWidget::update(Action* action) {
    using ConflictStatus = core::conflict::ConflictStatus;

    clear_layout(m_scroll_layout);
    m_files.clear();
    m_action = action;

    if (action == nullptr) {
        return;
    }
    Action* parent = action->get_prev();

    diff_result_t res;
    if (parent == nullptr) {
        git_commit* root_commit = action::ActionsManager::get().get_root_commit();
        res                     = core::git::prepare_diff(root_commit, action->get_commit());
    } else {
        switch (parent->get_tree_status()) {
        case ConflictStatus::UNKNOWN:
        case ConflictStatus::ERR:
            return;
        case ConflictStatus::HAS_CONFLICT:
        case ConflictStatus::NO_CONFLICT:
        case ConflictStatus::RESOLVED_CONFLICT:
            break;
        }

        if (parent->get_tree() == nullptr) {
            return;
        }

        res = core::git::prepare_resolution_diff(parent->get_tree(), action->get_commit());
    }

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
        createFileDiff(file_diff);
    }
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
    case diff_files_t::State::RENAMED:
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

void DiffWidget::createFileDiff(const diff_files_t& diff) {
    auto* file_diff = new DiffFile(diff);
    m_curr_editor   = file_diff->getEditor();

    QString header = create_diff_header(diff);
    if (header.isEmpty()) {
        const std::string& path = (!diff.old_file.path.empty()) ? diff.old_file.path : diff.new_file.path;

        LOG_ERROR("Unsupported file ({}) state: {}", path, static_cast<int>(diff.state));
        QMessageBox::critical(
            this,
            "Repository error",
            QString("Unsupported file '%1' has unsupported state: %2")
                .arg(QString::fromStdString(path))
                .arg(QString::number(static_cast<int>(diff.state)))
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
        text_section.format.setForeground(style::DiffStyle::get_color(section.type));

        if (section.type == section_t::Type::INFO) {
            text_section.format.setFontWeight(QFont::Bold);
        }

        text_section.format.setProperty(QTextFormat::FullWidthSelection, true);
        text_sections.append(text_section);
    }

    m_curr_editor->setExtraSelections(text_sections);

    file_diff->updateEditorHeight();

    m_scroll_layout->addWidget(file_diff);
    m_files.push_back(file_diff);

    m_curr_editor->enableContextMenu(m_action != nullptr);

    connect(m_curr_editor, &DiffEditor::extendContextMenu, this, [this](QMenu* menu) {
        menu->addSeparator();
        auto* split_act = menu->addAction("Split commit");

        connect(split_act, &QAction::triggered, this, &DiffWidget::splitCommitEvent);
    });
}

void DiffWidget::addHunkDiff(const diff_hunk_t& hunk, std::vector<section_t>& sections) {

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

    m_curr_editor->appendPlainText(hunk_info);

    auto* document = m_curr_editor->document();
    QTextCursor cursor(document);
    cursor.movePosition(QTextCursor::MoveOperation::End);

    QTextBlock block = cursor.block();

    sections.push_back(
        {
            .type  = section_t::Type::INFO,
            .block = block,
        }
    );

    for (const auto& line : hunk.lines) {
        addLineDiff(hunk, line, sections);
    }
}

void DiffWidget::addLineDiff(const diff_hunk_t& hunk, const diff_line_t& line, std::vector<section_t>& sections) {
    std::string new_content = line.content;
    new_content             = ' ' + new_content;

    m_curr_editor->appendPlainText(QString::fromStdString(new_content));

    auto* document = m_curr_editor->document();

    QTextCursor cursor(document);
    cursor.movePosition(QTextCursor::MoveOperation::End);

    QTextBlock block = cursor.block();
    block.setUserData(new DiffEditorLineData(line, hunk));

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

    sections.push_back(
        {
            .type  = type,
            .block = block,
        }
    );
}

void DiffWidget::splitCommitEvent() {
    using FileState = core::patch::PatchSplitter::FileState;

    std::string patch_text;

    {
        core::patch::PatchSplitter splitter;

        for (auto&& file : m_files) {
            const DiffEditor* editor = file->getEditor();

            if (!editor->selectedLineOrFile()) {
                continue;
            }

            FileState state = FileState::ALL_SELECTED;
            if (!splitter.file_begin(file->getDiff(), state)) {
                return;
            }

            std::vector<diff_line_t> lines;

            editor->processLines([&](const DiffEditorLineData& line) {
                splitter.process(line.get_line(), line.get_hunk(), line.is_selected());
            });

            splitter.file_end();
        }

        if (splitter.is_whole_patch()) {
            QMessageBox::critical(
                this, "Invalid Split", "The split must contain a non-empty subset of the patch, not entire patch"
            );
            return;
        }

        patch_text = splitter.get_patch();
    }

    auto handler = [this]() {
        const auto* err = git_error_last();
        if (err != nullptr && err->message != nullptr) {
            QMessageBox::critical(this, "Failed to create patch", err->message);
        } else {
            QMessageBox::critical(this, "Failed to create patch", "Unknown error");
        }
    };

    core::git::diff_t diff;

    int state = git_diff_from_buffer(&diff, patch_text.c_str(), patch_text.size());
    if (state != 0) {
        handler();
        return;
    }

    core::git::commit_t first_commit;
    core::git::commit_t second_commit;

    bool res = core::patch::split(first_commit, second_commit, m_action, diff);
    if (!res) {
        handler();
        return;
    }

    std::size_t index = action::ActionsManager::get().get_index(m_action);

    auto cmd = std::make_unique<CommitSplitCommand>(index, std::move(first_commit), std::move(second_commit));
    cmd->execute();

    core::state::CommandHistory::Add(std::move(cmd));
}

void CommitSplitCommand::execute() {

    LOG_INFO("Splitting commit");

    auto& manager = action::ActionsManager::get();
    auto* act     = manager.get_action(m_index);

    m_commit = manager.split(act, std::move(m_split.first), std::move(m_split.second));

    App::updateActions();
}

void CommitSplitCommand::undo() {

    LOG_INFO("Reverting the commit split operation");
    auto& manager = action::ActionsManager::get();
    auto* act     = manager.get_action(m_index);

    m_split = manager.merge_next(act, std::move(m_commit));

    App::updateActions();
}
}
