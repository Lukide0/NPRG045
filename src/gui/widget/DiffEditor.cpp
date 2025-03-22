#include "gui/widget/DiffEditor.h"
#include "core/git/diff.h"
#include "gui/widget/DiffEditorLine.h"

#include <QPainter>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QWidget>

// TODO: Line numbers

DiffEditor::DiffEditor(QWidget* parent)
    : QPlainTextEdit(parent) {

    m_line = new DiffEditorLine(this);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);

    connect(this, &DiffEditor::blockCountChanged, this, &DiffEditor::updateDiffLineWidth);
    connect(this, &DiffEditor::updateRequest, this, &DiffEditor::updateDiffLine);

    updateDiffLineWidth();

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

            auto* line_data = reinterpret_cast<DiffEditorLineData*>(block.userData());
            if (line_data != nullptr) {
                const auto* line = line_data->get_line();

                QString str;
                str += DiffEditorLine::ConvertToSymbol(line->type);
                painter.setPen(DiffEditorLine::ConvertToColor(line->type));
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
