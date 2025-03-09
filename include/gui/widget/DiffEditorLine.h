#pragma once

#include "core/git/diff.h"
#include "gui/widget/DiffEditor.h"
#include <QSize>
#include <QTextBlockUserData>
#include <QWidget>

class DiffEditorLineData : public QTextBlockUserData {
public:
    DiffEditorLineData(const diff_line_t* line, const diff_hunk_t* hunk)
        : m_line(line)
        , m_hunk(hunk) { }

    ~DiffEditorLineData() override = default;

    [[nodiscard]] const diff_line_t* get_line() const { return m_line; }

    [[nodiscard]] const diff_hunk_t* get_hunk() const { return m_hunk; }

private:
    const diff_line_t* m_line;
    const diff_hunk_t* m_hunk;
};

class DiffEditorLine : public QWidget {
public:
    DiffEditorLine(DiffEditor* editor)
        : QWidget(editor)
        , m_editor(editor) { }

    enum class Type {
        ADDITION,
        DELETION,
        HUNK_INFO,
    };

    [[nodiscard]] QSize sizeHint() const override { return QSize(m_editor->diffLineWidth(), 0); }

    static QColor ConvertToColor(Type type);
    static QColor ConvertToColor(diff_line_t::Type type);
    static char ConvertToSymbol(diff_line_t::Type type);

private:
    void paintEvent(QPaintEvent* event) override { m_editor->diffLinePaintEvent(event); }

private:
    DiffEditor* m_editor;
};
