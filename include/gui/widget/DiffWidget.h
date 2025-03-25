#pragma once

#include "core/git/diff.h"
#include "gui/color.h"
#include "gui/widget/DiffEditor.h"
#include "gui/widget/DiffFile.h"
#include "gui/widget/graph/Node.h"
#include <QScrollArea>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <vector>

class DiffWidget : public QWidget {
public:
    DiffWidget(QWidget* parent = nullptr);
    ~DiffWidget() override = default;

    void update(Node* node);

    [[nodiscard]] const std::vector<diff_files_t>& getDiffs() const { return m_diffs; }

    DiffFile* getDiffFile(std::size_t i) { return m_files[i]; }

    void ensureEditorVisible(DiffFile* file);

private:
    Node* m_node = nullptr;
    std::vector<diff_files_t> m_diffs;
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
        int end;
    };

    void createFileDiff(const diff_files_t& diff);
    void addHunkDiff(const diff_hunk_t& hunk, std::vector<section_t>& sections);
    void addLineDiff(const diff_hunk_t& hunk, const diff_line_t& line, std::vector<section_t>& sections);
};
