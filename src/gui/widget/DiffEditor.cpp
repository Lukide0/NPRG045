#include "gui/widget/DiffEditor.h"
#include "core/git/diff.h"
#include "gui/color.h"
#include "gui/widget/DiffEditorLine.h"

#include <iostream>
#include <QPainter>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QWidget>

DiffEditor::DiffEditor(QWidget* parent)
    : QPlainTextEdit(parent) {

    m_line = new DiffEditorLine(this);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);

    connect(this, &DiffEditor::blockCountChanged, this, &DiffEditor::updateDiffLineWidth);
    connect(this, &DiffEditor::updateRequest, this, &DiffEditor::updateDiffLine);
    connect(this, &QPlainTextEdit::selectionChanged, this, &DiffEditor::onSelectionChanged);

    updateDiffLineWidth();
    setContentsMargins(0, 0, 0, 0);

    setReadOnly(true);
}

int DiffEditor::diffLineWidth() {
    int space = 5 + fontMetrics().horizontalAdvance('-');
    return space;
}

void DiffEditor::updateDiffLineWidth(int /*unused*/) { setViewportMargins(diffLineWidth(), 0, 0, 0); }

void DiffEditor::updateDiffLine(const QRect& rect, int dy) {
    if (dy != 0) {
        m_line->scroll(0, dy);
    } else {
        m_line->update(0, rect.y(), m_line->width(), m_line->height());
    }

    if (rect.contains(viewport()->rect())) {
        updateDiffLineWidth();
    }
}

void DiffEditor::onSelectionChanged() {

    QTextCursor cursor = textCursor();

    if (!cursor.hasSelection()) {
        return;
    }

    QSignalBlocker blocker(this);

    int anchor = cursor.anchor();
    int pos    = cursor.position();

    auto anchor_block = document()->findBlock(anchor);
    auto block        = document()->findBlock(pos);

    int new_anchor;
    int new_pos;

    if (pos >= anchor) {
        new_anchor = anchor_block.position();
        new_pos    = block.position() + block.length() - 1;
    } else {

        new_anchor = anchor_block.position() + anchor_block.length() - 1;
        new_pos    = block.position();
    }

    cursor.setPosition(new_anchor);
    cursor.setPosition(new_pos, QTextCursor::KeepAnchor);

    setTextCursor(cursor);
}

void DiffEditor::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);

    QRect rect = contentsRect();
    rect.setWidth(diffLineWidth());

    m_line->setGeometry(rect);
}

void DiffEditor::diffLinePaintEvent(QPaintEvent* event) {
    auto painter = QPainter(m_line);

    QTextBlock block = firstVisibleBlock();
    int block_num    = block.blockNumber();
    int top          = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom       = top + qRound(blockBoundingRect(block).height());

    auto event_rect   = event->rect();
    auto event_top    = event_rect.top();
    auto event_bottom = event_rect.bottom();

    while (block.isValid() && top <= event_bottom) {
        if (block.isVisible() && bottom >= event_top) {

            auto* line_data = dynamic_cast<DiffEditorLineData*>(block.userData());
            if (line_data != nullptr) {
                const auto& line = line_data->get_line();

                QString str;
                str += convert_to_symbol(line.type);
                painter.setPen(convert_to_color(line.type));
                painter.drawText(0, top, m_line->width(), fontMetrics().height(), Qt::AlignCenter, str);
            }
        }

        // Next block
        block     = block.next();
        top       = bottom;
        bottom    = bottom + qRound(blockBoundingRect(block).height());
        block_num = block_num + 1;
    }
}
