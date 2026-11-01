#pragma once

#include "app/rebase/RebasePlanModel.h"
#include "core/action/Action.h"

#include <QAbstractItemModel>
#include <QObject>
#include <QStyleOptionViewItem>
#include <Qt>

#include <QtWidgets/QStyledItemDelegate>

namespace gui::rebase {

class RebaseActionDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    using ActionType    = core::action::ActionType;
    using ConflictState = core::conflict::ConflictState;

    explicit RebaseActionDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void setEditorData(QWidget* editor, const QModelIndex& index) const override;

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

    void
    updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    bool editorEvent(
        QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index
    ) override;

    bool helpEvent(
        QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index
    ) override;

signals:
    void editRequested(const QModelIndex& index);

private:
    static constexpr int ComboWidth        = 80;
    static constexpr int MarkerWidth       = 14;
    static constexpr int AffectMarkerWidth = 10;
    static constexpr int HorizontalSpacing = 3;
    static constexpr int LeftPadding       = 2;
    static constexpr int RightPadding      = 2;

    [[nodiscard]] static QRect comboRect(const QRect& rowRect);
    [[nodiscard]] static QRect markerRect(const QRect& rowRect);
    [[nodiscard]] static QRect affectMarkerRect(const QRect& rowRect);
    [[nodiscard]] static QRect messageRect(const QRect& rowRect);

    [[nodiscard]] static QString messageForIndex(const QModelIndex& index);
    [[nodiscard]] static QString typeText(ActionType type);
    [[nodiscard]] static QColor conflictColor(ConflictState state);

    static void drawConflictMarker(QPainter* painter, const QRect& rect, ConflictState state, const QColor& color);
    static void drawComboBox(QPainter* painter, const QRect& rect, const QString& text, const QFont& font);
    static void drawAffectMarker(QPainter* painter, const QRect& rect);
};
}
