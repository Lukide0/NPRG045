#pragma once

#include "core/git/diff.h"
#include "gui/widget/DiffEditor.h"
#include <QSize>
#include <QTextBlockUserData>
#include <QWidget>

namespace gui::widget {

class DiffEditorLineData : public QTextBlockUserData {
public:
    DiffEditorLineData(const core::git::diff_line_t& line, const core::git::diff_hunk_t& hunk)
        : m_line(line)
        , m_hunk(hunk) { }

    ~DiffEditorLineData() override = default;

    [[nodiscard]] const core::git::diff_line_t& get_line() const { return m_line; }

    [[nodiscard]] const core::git::diff_hunk_t& get_hunk() const { return m_hunk; }

    [[nodiscard]] bool is_selected() const { return m_selected; }

    void set_select(bool enable) { m_selected = enable; }

private:
    const core::git::diff_line_t& m_line;
    const core::git::diff_hunk_t& m_hunk;
    bool m_selected = false;
};

class DiffEditorLine : public QWidget {
public:
    DiffEditorLine(DiffEditor* editor)
        : QWidget(editor)
        , m_editor(editor) { }

    [[nodiscard]] QSize sizeHint() const override { return { m_editor->diffLineWidth(), 0 }; }

private:
    DiffEditor* m_editor;

    void paintEvent(QPaintEvent* event) override { m_editor->diffLinePaintEvent(event); }
};

}
