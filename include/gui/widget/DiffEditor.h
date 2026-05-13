#pragma once

#include "git/diff.h"

#include <cstddef>
#include <cstdint>
#include <functional>

#include <QObject>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QWidget>

namespace gui::widget {

class DiffEditorLine;
class DiffEditorLineData;

class DiffEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    DiffEditor(const git::diff_files_t& diff, QWidget* parent = nullptr);

    void diffLinePaintEvent(QPaintEvent* event);
    int diffLineWidth();

    void processLines(std::function<void(const DiffEditorLineData&)> process_data) const;

    void enableContextMenu(bool enable) { m_context_menu = enable; }

    [[nodiscard]] bool selectedLineOrFile() const { return m_selected_file || m_selected_count > 0; }

signals:
    void extendContextMenu(QMenu* menu);

    void lineOrFileSelect(bool selected);

private:
    static constexpr int HIGHLIGHT_SELECTION = 0x1;

    enum class SelectionType {
        SELECT,
        DESELECT,
    };

    void selectLine(SelectionType type);

    void selectFile(SelectionType type);

    void selectLines(SelectionType type);

    void selectHunk(SelectionType type);

    void selectBlock(QTextBlock block, SelectionType type);

    void resizeEvent(QResizeEvent* event) override;

    void contextMenuEvent(QContextMenuEvent* event) override;

    void updateDiffLineWidth(int new_block_count = 0);

    void onSelectionChanged();

    void updateDiffLine(const QRect& rect, int dy);

    void setBlockHighlight(QTextBlock block, bool enable);

    [[nodiscard]] bool selectOnlyFile() const;

private:
    const git::diff_files_t& m_diff;

    DiffEditorLine* m_line;
    bool m_context_menu           = false;
    std::int32_t m_selected_count = 0;
    bool m_selected_file          = false;
    std::size_t m_highlight_size  = 0;

    QPoint m_context_menu_point;
};

}
