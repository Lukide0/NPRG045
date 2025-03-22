#pragma once

#include "core/git/diff.h"
#include "gui/widget/DiffEditor.h"
#include "gui/widget/DiffEditorLine.h"
#include "gui/widget/graph/Node.h"
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

private:
    Node* m_node = nullptr;
    std::vector<diff_files_t> m_diffs;
    std::vector<DiffEditor*> m_editors;
    QVBoxLayout* m_layout;
    DiffEditor* m_curr_editor;

    struct section_t {
        using Type = DiffEditorLine::Type;

        Type type;
        int start;
        int end;
    };

    void createFileDiff(const diff_files_t& diff);
    void addHunkDiff(const diff_hunk_t& hunk, std::vector<section_t>& sections);
    void addLineDiff(const diff_hunk_t& hunk, const diff_line_t& line, std::vector<section_t>& sections);
};
