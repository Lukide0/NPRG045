#pragma once

#include "core/git/diff.h"
#include "gui/color.h"
#include "gui/widget/DiffEditor.h"
#include "gui/widget/DiffFile.h"

#include <cstddef>
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

    void update(git_commit* child, git_commit* parent);

    [[nodiscard]] const std::vector<core::git::diff_files_t>& getDiffs() const { return m_diffs; }

    DiffFile* getDiffFile(std::size_t i) { return m_files[i]; }

    void ensureEditorVisible(DiffFile* file);

private:
    std::vector<core::git::diff_files_t> m_diffs;
    std::vector<DiffFile*> m_files;
    QVBoxLayout* m_scroll_layout;
    QVBoxLayout* m_layout;
    QScrollArea* m_scrollarea;

    QWidget* m_scroll_content;
    DiffEditor* m_curr_editor;

    struct section_t {
        using Type = ColorType;

        Type type;
        int start;
    };

    void createFileDiff(const core::git::diff_files_t& diff);
    void addHunkDiff(const core::git::diff_hunk_t& hunk, std::vector<section_t>& sections);
    void addLineDiff(
        const core::git::diff_hunk_t& hunk, const core::git::diff_line_t& line, std::vector<section_t>& sections
    );

    void splitCommitEvent();
};

}
