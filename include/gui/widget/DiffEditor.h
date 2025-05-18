#pragma once

#include "core/git/diff.h"
#include <QPlainTextEdit>
#include <vector>

class DiffEditorLine;

class DiffEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    static constexpr int BLOCK_SELECTED = 0x1;

    DiffEditor(QWidget* parent = nullptr);

    void diffLinePaintEvent(QPaintEvent* event);
    int diffLineWidth();

    void getSelectedLines(std::vector<diff_line_t>& out_lines);

    enum class LinesActionType {
        SELECT,
        DESELECT,
    };
    void selectLines(LinesActionType type);

signals:
    void extendContextMenu(QMenu* menu);

private:
    void resizeEvent(QResizeEvent* event) override;

    void contextMenuEvent(QContextMenuEvent* event) override;

    void updateDiffLineWidth(int new_block_count = 0);
    void onSelectionChanged();

    void updateDiffLine(const QRect& rect, int dy);

    void setBlockHighlight(QTextBlock block, bool enable);

private:
    DiffEditorLine* m_line;
};
