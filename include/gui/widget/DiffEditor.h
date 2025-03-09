#pragma once

#include <QPlainTextEdit>

class DiffEditorLine;

class DiffEditor : public QPlainTextEdit {
public:
    DiffEditor(QWidget* parent = nullptr);

    void diffLinePaintEvent(QPaintEvent* event);
    int diffLineWidth();

private:
    void resizeEvent(QResizeEvent* event) override;

    void updateDiffLineWidth(int new_block_count = 0);
    // void highlightCurrentLine();
    void updateDiffLine(const QRect& rect, int dy);

private:
    DiffEditorLine* m_line;
};
