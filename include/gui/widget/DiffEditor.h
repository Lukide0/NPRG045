#pragma once

#include "core/git/diff.h"
#include <functional>
#include <QPlainTextEdit>
#include <vector>

namespace gui::widget {

class DiffEditorLine;
class DiffEditorLineData;

class DiffEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    DiffEditor(QWidget* parent = nullptr);

    void diffLinePaintEvent(QPaintEvent* event);
    int diffLineWidth();

    void processLines(std::function<void(const DiffEditorLineData&)> process_data);

    void enableContextMenu(bool enable) { m_context_menu = enable; }

signals:
    void extendContextMenu(QMenu* menu);

private:
    static constexpr int HIGHLIGHT_SELECTION = 0x1;

    enum class SelectionType {
        SELECT,
        DESELECT,
    };

    void selectLine(SelectionType type);

    void selectLines(SelectionType type);

    void selectHunk(SelectionType type);

    void selectBlock(QTextBlock block, SelectionType type);

    void resizeEvent(QResizeEvent* event) override;

    void contextMenuEvent(QContextMenuEvent* event) override;

    void updateDiffLineWidth(int new_block_count = 0);
    void onSelectionChanged();

    void updateDiffLine(const QRect& rect, int dy);

    void setBlockHighlight(QTextBlock block, bool enable);

private:
    DiffEditorLine* m_line;
    bool m_context_menu          = false;
    std::size_t m_highlight_size = 0;

    QPoint m_context_menu_point;
};

}
