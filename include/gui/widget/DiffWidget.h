#pragma once

#include "action/Action.h"
#include "git/diff.h"
#include "git/types.h"
#include "gui/color.h"
#include "gui/style/DiffStyle.h"
#include "gui/widget/DiffEditor.h"
#include "gui/widget/DiffFile.h"
#include "state/Command.h"

#include <cstddef>
#include <utility>
#include <vector>

#include <git2/types.h>

#include <QScrollArea>
#include <QTextBlock>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::widget {

class DiffWidget : public QWidget {
public:
    DiffWidget(QWidget* parent = nullptr);
    ~DiffWidget() override = default;

    void update(action::Action* act);
    void update(git_commit* commit);

    void clear();

    [[nodiscard]] const std::vector<git::diff_files_t>& getDiffs() const { return m_diffs; }

    DiffFile* getDiffFile(std::size_t i) { return m_files[i]; }

    void ensureEditorVisible(DiffFile* file);

private:
    action::Action* m_action = nullptr;

    std::vector<git::diff_files_t> m_diffs;
    std::vector<DiffFile*> m_files;
    QVBoxLayout* m_scroll_layout;
    QVBoxLayout* m_layout;
    QScrollArea* m_scrollarea;

    QWidget* m_scroll_content;
    DiffEditor* m_curr_editor;

    struct section_t {
        using Type = style::DiffStyle::Style;

        Type type;
        QTextBlock block;
    };

    void update(git::diff_result_t& res, bool editable);

    void createFileDiff(const git::diff_files_t& diff, bool editable);
    void addHunkDiff(const git::diff_hunk_t& hunk, std::vector<section_t>& sections);
    void addLineDiff(
        QTextCursor& cursor,
        const git::diff_hunk_t& hunk,
        const git::diff_line_t& line,
        std::vector<section_t>& sections
    );

    void splitCommitEvent();
};

class CommitSplitCommand : public state::Command {
public:
    CommitSplitCommand(std::size_t index, git::commit_t&& first, git::commit_t&& second)
        : m_index(index)
        , m_split(std::move(first), std::move(second)) { }

    ~CommitSplitCommand() override = default;

    void execute() override;
    void undo() override;

private:
    std::size_t m_index;
    std::pair<git::commit_t, git::commit_t> m_split;
    git::commit_t m_commit;
};

}
