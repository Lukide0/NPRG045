#include "gui/widget/DiffEditor.h"
#include "core/git/diff.h"
#include "gui/color.h"
#include "gui/widget/DiffEditorLine.h"

#include <functional>

#include <QColor>
#include <QFrame>
#include <QMenu>
#include <qnamespace.h>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTextBlock>
#include <qtmetamacros.h>
#include <QtNumeric>
#include <QWidget>

namespace gui::widget {

using core::git::diff_line_t;

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

    auto pal = palette();
    pal.setColor(QPalette::Highlight, get_highlight_color());

    setPalette(pal);
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

    auto selections = extraSelections();
    selections.resize(selections.size() - m_highlight_size);
    m_highlight_size = 0;

    if (!cursor.hasSelection()) {
        setExtraSelections(selections);
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

    auto start = cursor.selectionStart();
    auto end   = cursor.selectionEnd();

    auto selected_block = document()->findBlock(start);
    while (selected_block.isValid() && selected_block.position() <= end) {
        QTextEdit::ExtraSelection selection;

        selection.cursor = QTextCursor(selected_block);
        selection.format.setBackground(palette().highlight());
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.format.setProperty(QTextFormat::UserProperty, HIGHLIGHT_SELECTION);

        selections.append(selection);
        m_highlight_size += 1;

        selected_block = selected_block.next();
    }

    setExtraSelections(selections);
}

void DiffEditor::contextMenuEvent(QContextMenuEvent* event) {
    auto* menu = new QMenu();

    m_context_menu_point = event->globalPos();

    if (m_context_menu) {
        auto cursor = textCursor();

        if (cursor.hasSelection()) {
            menu->addSection("Lines");
            QAction* select_lines   = menu->addAction("Select lines");
            QAction* deselect_lines = menu->addAction("Deselect lines");

            connect(select_lines, &QAction::triggered, this, [this]() { this->selectLines(SelectionType::SELECT); });
            connect(deselect_lines, &QAction::triggered, this, [this]() {
                this->selectLines(SelectionType::DESELECT);
            });
        } else {
            menu->addSection("Line");
            QAction* select_line   = menu->addAction("Select line");
            QAction* deselect_line = menu->addAction("Deselect line");

            connect(select_line, &QAction::triggered, this, [this]() { this->selectLine(SelectionType::SELECT); });
            connect(deselect_line, &QAction::triggered, this, [this]() { this->selectLine(SelectionType::DESELECT); });
        }

        menu->addSection("Hunk");
        QAction* select_hunk   = menu->addAction("Select hunk");
        QAction* deselect_hunk = menu->addAction("Deselect hunk");

        connect(select_hunk, &QAction::triggered, this, [this]() { this->selectHunk(SelectionType::SELECT); });
        connect(deselect_hunk, &QAction::triggered, this, [this]() { this->selectHunk(SelectionType::DESELECT); });

        emit extendContextMenu(menu);
    }

    menu->exec(m_context_menu_point);

    delete menu;

    event->accept();
}

void DiffEditor::processLines(std::function<void(const DiffEditorLineData&)> process_data) {
    auto* doc = document();

    for (auto block = doc->begin(); block.isValid() && block != doc->end(); block = block.next()) {
        auto* user_data = block.userData();

        if (auto* line_data = dynamic_cast<DiffEditorLineData*>(user_data)) {
            process_data(*line_data);
        }
    }
}

void DiffEditor::selectLine(SelectionType type) {

    auto viewport_pos  = viewport()->mapFromGlobal(m_context_menu_point);
    QTextCursor cursor = cursorForPosition(viewport_pos);

    auto block = cursor.block();

    if (!block.isValid()) {
        return;
    }

    selectBlock(block, type);
}

void DiffEditor::selectBlock(QTextBlock block, SelectionType type) {
    auto* line_data = dynamic_cast<DiffEditorLineData*>(block.userData());
    if (line_data == nullptr) {
        return;
    }

    auto line_type = line_data->get_line().type;

    switch (line_type) {
    case diff_line_t::Type::CONTEXT:
    case diff_line_t::Type::CONTEXT_NO_NEWLINE:
        break;
    case diff_line_t::Type::ADDITION:
    case diff_line_t::Type::ADDITION_NEWLINE:
    case diff_line_t::Type::DELETION:
    case diff_line_t::Type::DELETION_NEWLINE:
        setBlockHighlight(block, type == SelectionType::SELECT);
        break;
    }
}

void DiffEditor::selectLines(SelectionType type) {
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        return;
    }

    int start = cursor.selectionStart();
    int end   = cursor.selectionEnd();

    auto start_block = document()->findBlock(start);
    auto end_block   = document()->findBlock(end);

    cursor.clearSelection();

    for (auto block = start_block; block.isValid() && block.blockNumber() <= end_block.blockNumber();
         block      = block.next()) {

        selectBlock(block, type);
    }
}

void DiffEditor::selectHunk(SelectionType type) {
    QTextCursor cursor = textCursor();
    cursor.clearSelection();

    auto curr_block = cursor.block();
    auto* line_data = dynamic_cast<DiffEditorLineData*>(curr_block.userData());

    if (!curr_block.isValid() || line_data == nullptr) {
        return;
    }

    selectBlock(curr_block, type);
    const auto& hunk = line_data->get_hunk();

    auto block = curr_block.previous();
    while (block.isValid()) {
        auto* block_data = dynamic_cast<DiffEditorLineData*>(block.userData());

        // compare hunk poiters
        if (block_data == nullptr || &block_data->get_hunk() != &hunk) {
            break;
        }

        selectBlock(block, type);
        block = block.previous();
    }

    block = curr_block.next();
    while (block.isValid()) {
        auto* block_data = dynamic_cast<DiffEditorLineData*>(block.userData());

        // compare hunk poiters
        if (block_data == nullptr || &block_data->get_hunk() != &hunk) {
            break;
        }

        selectBlock(block, type);
        block = block.next();
    }
}

void DiffEditor::setBlockHighlight(QTextBlock block, bool enable) {
    auto selections = extraSelections();

    auto it = selections.begin();
    for (; it != selections.end(); ++it) {
        if (it->cursor.block() == block && it->format.property(QTextFormat::UserProperty) != HIGHLIGHT_SELECTION) {
            break;
        }
    }

    auto* line_data = dynamic_cast<DiffEditorLineData*>(block.userData());

    if (it == selections.end() || line_data == nullptr) {
        return;
    }

    auto line        = line_data->get_line();
    QColor highlight = convert_to_color(line.type);
    highlight.setAlpha(30);

    line_data->set_select(enable);
    if (enable) {
        it->format.setBackground(highlight);
    } else {
        it->format.clearBackground();
    }

    setExtraSelections(selections);
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

                if (line_data->is_selected()) {

                    painter.setPen(convert_to_color(line.type));
                } else {
                    painter.setPen(Qt::black);
                }

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

}
