#include "gui/rebase/RebaseActionDelegate.h"

#include "app/rebase/RebasePlanModel.h"

#include "core/action/Action.h"
#include "gui/style/ConflictStyle.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QTimer>
#include <QToolTip>
#include <QtWidgets/QStyledItemDelegate>

namespace gui::rebase {

using app::rebase::RebasePlanModel;

RebaseActionDelegate::RebaseActionDelegate(QObject* parent)
    : QStyledItemDelegate(parent) { }

QRect RebaseActionDelegate::comboRect(const QRect& rowRect) {
    return { rowRect.left() + LeftPadding, rowRect.top() + 1, ComboWidth, qMax(0, rowRect.height() - 2) };
}

QRect RebaseActionDelegate::markerRect(const QRect& rowRect) {
    const int left = rowRect.left() + LeftPadding + ComboWidth + HorizontalSpacing;
    return { left, rowRect.top(), MarkerWidth, rowRect.height() };
}

QRect RebaseActionDelegate::affectMarkerRect(const QRect& rowRect) {
    const int left = rowRect.left() + LeftPadding + ComboWidth + HorizontalSpacing + MarkerWidth + HorizontalSpacing;
    return { left, rowRect.top(), AffectMarkerWidth, rowRect.height() };
}

QRect RebaseActionDelegate::messageRect(const QRect& rowRect) {
    const int left  = rowRect.left() + LeftPadding + ComboWidth + HorizontalSpacing + MarkerWidth + HorizontalSpacing;
    const int width = qMax(0, rowRect.right() - RightPadding - left + 1);

    return { left, rowRect.top(), width, rowRect.height() };
}

QString RebaseActionDelegate::messageForIndex(const QModelIndex& index) {
    const QString msg = index.data(RebasePlanModel::MessageRole).toString();

    if (!msg.isEmpty()) {
        return msg;
    }

    return index.data(RebasePlanModel::SummaryRole).toString();
}

QString RebaseActionDelegate::typeText(ActionType type) {
    return QString::fromStdString(core::action::type_to_str(type));
}

QColor RebaseActionDelegate::conflictColor(ConflictState state) {
    using ConflictColor = style::ConflictStyle::Style;

    switch (state) {
    case ConflictState::Unknown:
    case ConflictState::Error:
        return style::ConflictStyle::getColor(ConflictColor::UNKNOWN);

    case ConflictState::Clean:
        return style::ConflictStyle::getColor(ConflictColor::NORMAL);

    case ConflictState::Conflict:
        return style::ConflictStyle::getColor(ConflictColor::CONFLICT);

    case ConflictState::Resolved:
        return style::ConflictStyle::getColor(ConflictColor::RESOLVED_CONFLICT);
    }

    return style::ConflictStyle::getColor(ConflictColor::UNKNOWN);
}

void RebaseActionDelegate::drawConflictMarker(
    QPainter* painter, const QRect& rect, ConflictState state, const QColor& color
) {
    painter->save();

    painter->setRenderHint(QPainter::Antialiasing);

    const QPoint center = rect.center();

    switch (state) {
    case ConflictState::Unknown:
    case ConflictState::Error: {
        const QRect marker_rect(center.x() - 7, center.y() - 7, 14, 14);

        painter->setPen(color);
        painter->drawText(
            marker_rect, Qt::AlignCenter, state == ConflictState::Error ? QStringLiteral("!") : QStringLiteral("?")
        );

        break;
    }

    case ConflictState::Clean:
        break;
    case ConflictState::Conflict: {
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);

        painter->drawEllipse(center, 5, 5);

        painter->setPen(QPen(Qt::white, 1.5));

        painter->drawLine(center.x(), center.y() - 3, center.x(), center.y() + 1);
        painter->drawPoint(center.x(), center.y() + 3);

        break;
    }

    case ConflictState::Resolved: {
        painter->setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        painter->drawLine(center.x() - 4, center.y(), center.x() - 1, center.y() + 3);
        painter->drawLine(center.x() - 1, center.y() + 3, center.x() + 5, center.y() - 4);

        break;
    }
    }

    painter->restore();
}

void RebaseActionDelegate::drawAffectMarker(QPainter* painter, const QRect& rect) {
    const QPoint center = rect.center();

    painter->save();

    QPen pen(conflictColor(ConflictState::Conflict));
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);

    painter->setPen(pen);

    // arrow

    painter->drawLine(QPoint(center.x() - 3, center.y()), QPoint(center.x() + 3, center.y()));
    painter->drawLine(QPoint(center.x(), center.y() - 2), QPoint(center.x() + 3, center.y()));
    painter->drawLine(QPoint(center.x(), center.y() + 2), QPoint(center.x() + 3, center.y()));

    painter->restore();
}

void RebaseActionDelegate::paint(
    QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index
) const {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    QStyle* style = (opt.widget != nullptr) ? opt.widget->style() : QApplication::style();

    // item background
    opt.text.clear();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const auto type          = static_cast<ActionType>(index.data(RebasePlanModel::TypeRole).toInt());
    const auto state         = static_cast<ConflictState>(index.data(RebasePlanModel::ConflictStateRole).toInt());
    const bool affect_marker = index.data(RebasePlanModel::AffectsLaterConflictRole).toBool();
    const QString message    = messageForIndex(index);
    const QColor color       = conflictColor(state);

    drawComboBox(painter, comboRect(option.rect), typeText(type), opt.font);
    drawConflictMarker(painter, markerRect(option.rect), state, color);

    QRect text_rect = messageRect(option.rect);

    if (affect_marker) {
        drawAffectMarker(painter, affectMarkerRect(option.rect));

        text_rect.adjust(AffectMarkerWidth + HorizontalSpacing, 0, 0, 0);
    }

    if (text_rect.width() <= 0) {
        return;
    }

    painter->save();

    painter->setFont(opt.font);
    painter->setPen(color);
    painter->setClipRect(text_rect);

    const QString elided_msg = opt.fontMetrics.elidedText(message, Qt::ElideRight, text_rect.width());

    painter->drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, elided_msg);

    painter->restore();
}

QSize RebaseActionDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QSize size = QStyledItemDelegate::sizeHint(option, index);

    constexpr int min_height       = 24;
    constexpr int vertical_padding = 6;

    const int text_height = option.fontMetrics.height() + vertical_padding;

    size.setHeight(qMax(min_height, text_height));

    return size;
}

QWidget* RebaseActionDelegate::
    createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const {
    auto* combo = new QComboBox(parent);

    for (const auto type : core::action::action_types) {
        combo->addItem(typeText(type), static_cast<int>(type));
    }

    auto* self = const_cast<RebaseActionDelegate*>(this);

    connect(combo, qOverload<int>(&QComboBox::activated), combo, [self, combo](int) {
        emit self->commitData(combo);
        emit self->closeEditor(combo);
    });

    QTimer::singleShot(0, combo, [combo] { combo->showPopup(); });

    return combo;
}

void RebaseActionDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    auto* combo = qobject_cast<QComboBox*>(editor);

    if (combo == nullptr) {
        return;
    }

    const int type = index.data(RebasePlanModel::TypeRole).toInt();

    const QSignalBlocker blocker(combo);

    combo->setCurrentIndex(combo->findData(type));
}

void RebaseActionDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
    auto* combo = qobject_cast<QComboBox*>(editor);

    if (combo == nullptr) {
        return;
    }

    model->setData(index, combo->currentData(), RebasePlanModel::TypeRole);
}

void RebaseActionDelegate::updateEditorGeometry(
    QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& /*index*/
) const {
    editor->setGeometry(comboRect(option.rect));
}

bool RebaseActionDelegate::editorEvent(
    QEvent* event, QAbstractItemModel* /*model*/, const QStyleOptionViewItem& option, const QModelIndex& index
) {
    if (event->type() != QEvent::MouseButtonPress || !index.isValid()) {
        return false;
    }

    const auto* mouse = static_cast<const QMouseEvent*>(event);

    if (mouse->button() != Qt::LeftButton) {
        return false;
    }

    const QPoint position = mouse->position().toPoint();

    if (!comboRect(option.rect).contains(position)) {
        return false;
    }

    emit editRequested(index);
    return true;
}

bool RebaseActionDelegate::helpEvent(
    QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index
) {
    if (!index.isValid()) {
        return false;
    }

    QStringList lines;

    const QString hash       = index.data(RebasePlanModel::HashRole).toString();
    const QString commitId   = index.data(RebasePlanModel::CommitIdRole).toString();
    const int files          = index.data(RebasePlanModel::ConflictFileCountRole).toInt();
    const int unresolved     = index.data(RebasePlanModel::UnresolvedCountRole).toInt();
    const QString error      = index.data(RebasePlanModel::ErrorRole).toString();
    const bool affect_marker = index.data(RebasePlanModel::AffectsLaterConflictRole).toBool();

    if (!hash.isEmpty()) {
        lines << QStringLiteral("Commit: %1").arg(hash);
    }

    if (!commitId.isEmpty()) {
        lines << QStringLiteral("Full ID: %1").arg(commitId);
    }

    if (files > 0) {
        lines << QStringLiteral("Conflicting files: %1").arg(files);
        lines << QStringLiteral("Unresolved files: %1").arg(unresolved);
    }

    if (!error.isEmpty()) {
        lines << QStringLiteral("Error: %1").arg(error);
    }

    if (affect_marker) {
        lines << QStringLiteral("This action modifies a file involved in a later conflict.");
    }

    if (lines.isEmpty()) {
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }

    QToolTip::showText(event->globalPos(), lines.join(QStringLiteral("\n")), view->viewport(), option.rect);

    return true;
}

void RebaseActionDelegate::drawComboBox(QPainter* painter, const QRect& rect, const QString& text, const QFont& font) {
    if (rect.isEmpty()) {
        return;
    }

    static const QColor BorderColor("#263238");
    static const QColor BackgroundColor("#FAFAFA");
    static const QColor TextColor("#263238");
    static const QPixmap arrow(QStringLiteral(":/icons/arrow-down-16x16.png"));

    constexpr int dropdown_width = 20;
    constexpr int text_padding   = 7;
    constexpr int arrow_size     = 16;

    painter->save();

    painter->setRenderHint(QPainter::Antialiasing, false);

    // border + background
    painter->fillRect(rect, BackgroundColor);

    painter->setPen(QPen(BorderColor, 1));
    painter->setBrush(Qt::NoBrush);

    painter->drawRect(rect.adjusted(0, 0, -1, -1));

    // dropdown arrow
    const int separator = rect.right() - dropdown_width + 1;
    painter->drawLine(separator, rect.top() + 1, separator, rect.bottom() - 1);

    const QRect dropdown_rect(separator + 1, rect.top() + 1, rect.right() - separator - 1, rect.height() - 2);

    if (!arrow.isNull()) {
        QRect arrowRect(QPoint(0, 0), QSize(arrow_size, arrow_size));
        arrowRect.moveCenter(dropdown_rect.center());
        painter->drawPixmap(arrowRect, arrow);
    } else {
        // fallback
        const QPoint center = dropdown_rect.center();

        painter->setPen(QPen(BorderColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawLine(center.x() - 4, center.y() - 2, center.x(), center.y() + 2);
        painter->drawLine(center.x(), center.y() + 2, center.x() + 4, center.y() - 2);
    }

    // text
    const QRect text_rect(
        rect.left() + text_padding,
        rect.top() + 1,
        qMax(0, separator - rect.left() - text_padding - 2),
        rect.height() - 2
    );

    painter->setFont(font);
    painter->setPen(TextColor);
    painter->setClipRect(text_rect);

    const QFontMetrics metrics(font);

    const QString elided_text = metrics.elidedText(text, Qt::ElideRight, text_rect.width());

    painter->drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, elided_text);
    painter->restore();
}

}
