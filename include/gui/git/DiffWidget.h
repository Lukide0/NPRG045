#pragma once

#include "app/rebase/RebaseController.h"
#include "core/git/diff.h"
#include "core/rebase/RebaseAnalysis.h"
#include "core/rebase/RebaseSession.h"
#include "gui/git/DiffEditor.h"
#include "gui/git/DiffFile.h"
#include "gui/style/DiffStyle.h"

#include <cstddef>
#include <vector>

#include <git2/types.h>

#include <QScrollArea>
#include <QTextBlock>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::git {

class DiffWidget : public QWidget {
public:
    DiffWidget(QWidget* parent = nullptr);
    ~DiffWidget() override = default;

    void setSession(core::rebase::RebaseSession* session, app::rebase::RebaseController* controller);

    void clear();

    [[nodiscard]] const std::vector<core::git::diff_files_t>& getDiffs() const { return m_diffs; }

    DiffFile* getDiffFile(std::size_t i) {
        if (i > m_files.size()) {
            return nullptr;
        }
        return m_files[i];
    }

    void ensureEditorVisible(DiffFile* file);

public slots:
    void setCurrentStep(core::rebase::RebaseAnalysisConstPtr analysis, int row);
    void setCurrentCommit(git_commit* commit);

private:
    core::rebase::RebaseSession* m_rebase = nullptr;
    core::rebase::RebaseAnalysisConstPtr m_analysis;
    app::rebase::RebaseController* m_controller = nullptr;
    const core::rebase::StepResult* m_step      = nullptr;
    const core::rebase::StepResult* m_prev_step = nullptr;

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

    void update(core::git::diff_result_t& res, bool editable);

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

}
