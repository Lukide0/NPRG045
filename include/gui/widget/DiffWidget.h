#pragma once

#include "core/action/Action.h"
#include "core/conflict/ConflictManager.h"
#include "core/git/diff.h"
#include "core/git/types.h"
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
    DiffWidget(core::action::ActionsManager& actions, core::conflict::ConflictManager& conflicts, QWidget* parent = nullptr);
    ~DiffWidget() override = default;

    void update(core::action::Action* act);
    void update(git_commit* commit);
    void update(core::git::diff_result_t& res, bool editable);

    void clear();

    [[nodiscard]] const std::vector<core::git::diff_files_t>& getDiffs() const { return m_diffs; }

    DiffFile* getDiffFile(std::size_t i) { return m_files[i]; }

    void ensureEditorVisible(DiffFile* file);

private:
    core::action::Action* m_action = nullptr;
    core::action::ActionsManager& m_manager;
    core::conflict::ConflictManager& m_conflicts;

    std::vector<core::git::diff_files_t> m_diffs;
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

    void createFileDiff(const core::git::diff_files_t& diff, bool editable);
    void addHunkDiff(const core::git::diff_hunk_t& hunk, std::vector<section_t>& sections);
    void addLineDiff(
        QTextCursor& cursor,
        const core::git::diff_hunk_t& hunk,
        const core::git::diff_line_t& line,
        std::vector<section_t>& sections
    );

    void splitCommitEvent();
};

class CommitSplitCommand : public state::Command {
public:
    CommitSplitCommand(
        std::size_t index, core::git::commit_t&& first, core::git::commit_t&& second, core::action::ActionsManager& manager
    )
        : m_index(index)
        , m_split(std::move(first), std::move(second))
        , m_manager(manager) { }

    ~CommitSplitCommand() override = default;

    void execute() override;
    void undo() override;

private:
    std::size_t m_index;
    std::pair<core::git::commit_t, core::git::commit_t> m_split;
    core::git::commit_t m_commit;
    core::action::ActionsManager& m_manager;
};

}
