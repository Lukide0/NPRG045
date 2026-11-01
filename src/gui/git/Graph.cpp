#include "gui/git/Graph.h"

#include "gui/style/ConflictStyle.h"
#include "gui/style/GlobalStyle.h"
#include "gui/style/StyleManager.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QPainter>
#include <QPalette>
#include <QRectF>
#include <QResizeEvent>
#include <QStyleOptionGraphicsItem>
#include <QTextOption>
#include <QtTypes>
#include <QWidget>

namespace gui::git {

struct GraphMetrics {
    QFont font;
    qreal hash_width = 0;
};

class GraphNode final : public QGraphicsItem {
public:
    static constexpr int Type       = UserType + 1;
    static constexpr qreal height   = 20;
    static constexpr qreal minWidth = 300;
    static constexpr qreal padding  = 2;
    static constexpr qreal gap      = 2;

    GraphNode(const GraphMetrics& metrics, GraphNodeData data)
        : m_metrics(metrics)
        , m_data(std::move(data)) {
        setFlag(QGraphicsItem::ItemIsFocusable);
        setToolTip(m_data.tooltip);
    }

    [[nodiscard]] int type() const override { return Type; }

    [[nodiscard]] const GraphNodeData& nodeData() const { return m_data; }

    [[nodiscard]] const QString& id() const { return m_data.id; }

    void setNodeData(GraphNodeData data) {
        m_data = std::move(data);
        setToolTip(m_data.tooltip);
        update();
    }

    void setState(GraphNodeData::GraphNodeState state) {
        if (m_data.state != state) {
            m_data.state = state;
            update();
        }
    }

    void setCurrent(bool current) {
        if (m_current != current) {
            m_current = current;
            update();
        }
    }

    void setWidth(qreal width) {
        width = std::max(minWidth, width);

        if (!qFuzzyCompare(width, m_width)) {
            prepareGeometryChange();
            m_width = width;
        }
    }

    [[nodiscard]] QRectF boundingRect() const override { return { 0, 0, m_width, height }; }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* widget) override {
        using style::ConflictStyle;
        using GraphNodeState = GraphNodeData::GraphNodeState;

        const QPalette palette = widget != nullptr ? widget->palette() : QApplication::palette();
        const QColor highlight = style::GlobalStyle::get_color(style::GlobalStyle::HIGHLIGHT);

        bool draw_box = true;
        QColor color  = highlight;

        switch (m_data.state) {
        case GraphNodeState::Clean:
            draw_box = m_current;
            break;
        case GraphNodeState::Unknown:
        case GraphNodeState::Error:
            color = ConflictStyle::get_color(ConflictStyle::UNKNOWN);
            break;
        case GraphNodeState::Conflict:
            color = ConflictStyle::get_color(ConflictStyle::CONFLICT);
            break;
        case GraphNodeState::Resolved:
            color = ConflictStyle::get_color(ConflictStyle::RESOLVED_CONFLICT);
            break;
        }

        const qreal hash_width = m_metrics.hash_width;
        const qreal text_left  = padding + hash_width + padding;

        painter->setFont(m_metrics.font);

        if (draw_box) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(color);
            painter->drawRect(QRectF { padding + hash_width, 0, m_width - hash_width - (2 * padding), height });
        }

        painter->setPen(palette.color(QPalette::Text));
        painter->setBrush(
            m_current ? highlight : (m_data.fill.isValid() ? m_data.fill : palette.color(QPalette::Base))
        );
        painter->drawRect(QRectF { padding, 0, hash_width, height });

        const QTextOption text_options(Qt::AlignLeft | Qt::AlignVCenter);

        painter->drawText(QRectF { padding * 2, 0, hash_width - padding, height }, m_data.hash, text_options);

        const qreal text_width = std::max<qreal>(0, m_width - text_left - padding);
        const QString message
            = painter->fontMetrics().elidedText(m_data.message, Qt::ElideRight, static_cast<int>(text_width));

        painter->drawText(QRectF { text_left, 0, text_width, height }, message, text_options);
    }

private:
    const GraphMetrics& m_metrics;
    GraphNodeData m_data;

    qreal m_width  = minWidth;
    bool m_current = false;
};

GraphWidget::GraphWidget(QWidget* parent)
    : QGraphicsView(parent)
    , m_metrics(std::make_unique<GraphMetrics>()) {

    m_metrics->font = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    auto* scene = new QGraphicsScene(this);
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    setScene(scene);

    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    scale(static_cast<qreal>(0.95), static_cast<qreal>(0.95));
    setViewportMargins(0, 0, 10, 0);

    connect(scene, &QGraphicsScene::focusItemChanged, this, &GraphWidget::onFocusItemChanged);

    connect(
        &style::StyleManager::get_conflict_style(),
        &style::ConflictStyle::changed,
        viewport(),
        qOverload<>(&QWidget::update)
    );
    connect(
        &style::StyleManager::get_global_style(),
        &style::GlobalStyle::changed,
        viewport(),
        qOverload<>(&QWidget::update)
    );

    relayout();
}

GraphWidget::~GraphWidget() = default;

void GraphWidget::setNodes(const QList<GraphNodeData>& nodes) {
    const QString keep = m_current_id;

    m_updating = true;
    destroyNodes();

    int next_row = 0;

    for (const GraphNodeData& source : nodes) {
        if (source.id.isEmpty() || m_by_id.contains(source.id)) {
            continue;
        }

        GraphNodeData node_data = source;
        if (node_data.row < 0) {
            node_data.row = next_row;
        }
        next_row = std::max(next_row, node_data.row + 1);

        auto* node = new GraphNode(*m_metrics, std::move(node_data));
        scene()->addItem(node);

        m_nodes.push_back(node);
        m_by_id.insert(node->id(), node);
    }

    relayout();

    m_current_id.clear();
    if (GraphNode* node = m_by_id.value(keep, nullptr); node != nullptr) {
        markCurrent(node);
        node->setFocus(Qt::OtherFocusReason);
    }

    m_updating = false;
}

void GraphWidget::clearNodes() {
    m_updating = true;

    destroyNodes();
    m_current_id.clear();
    relayout();

    m_updating = false;
}

bool GraphWidget::updateNode(const GraphNodeData& node_data) {
    GraphNode* node = m_by_id.value(node_data.id, nullptr);
    if (node == nullptr) {
        return false;
    }

    GraphNodeData merged = node_data;
    if (merged.row < 0) {
        merged.row = node->nodeData().row;
    }

    node->setNodeData(std::move(merged));

    relayout();
    return true;
}

bool GraphWidget::setNodeState(const QString& id, GraphNodeData::GraphNodeState state) {
    GraphNode* node = m_by_id.value(id, nullptr);
    if (node == nullptr) {
        return false;
    }

    node->setState(state);
    return true;
}

[[nodiscard]] const GraphNodeData* GraphWidget::nodeData(const QString& id) const {

    auto it = m_by_id.constFind(id);
    if (it != m_by_id.constEnd()) {
        return &it.value()->nodeData();
    }
    return nullptr;
}

bool GraphWidget::setCurrentId(const QString& id) {
    GraphNode* node = m_by_id.value(id, nullptr);
    if (node == nullptr) {
        return false;
    }

    markCurrent(node);

    m_updating = true;
    node->setFocus(Qt::OtherFocusReason);
    m_updating = false;

    ensureVisible(node, 0, static_cast<int>(GraphNode::gap));
    return true;
}

void GraphWidget::clearCurrent() {
    GraphNode* previous = m_by_id.value(m_current_id, nullptr);
    if (previous != nullptr) {
        previous->setCurrent(false);
    }
    m_current_id.clear();

    m_updating = true;
    scene()->setFocusItem(nullptr);
    m_updating = false;
}

void GraphWidget::onFocusItemChanged(QGraphicsItem* now, QGraphicsItem* /*old*/, Qt::FocusReason /*reason*/) {
    if (m_updating || now == nullptr) {
        return;
    }

    auto* node = qgraphicsitem_cast<GraphNode*>(now);
    if (node == nullptr || node->id() == m_current_id) {
        return;
    }

    const QString previous = m_current_id;
    markCurrent(node);

    emit currentChanged(node->id(), previous);
}

void GraphWidget::markCurrent(GraphNode* node) {
    if (GraphNode* previous = m_by_id.value(m_current_id, nullptr); previous != nullptr) {
        previous->setCurrent(false);
    }

    node->setCurrent(true);
    m_current_id = node->id();
}

void GraphWidget::destroyNodes() {
    m_nodes.clear();
    m_by_id.clear();
    scene()->clear();
}

void GraphWidget::relayout() {
    const QFontMetricsF font_metrics(m_metrics->font);

    qreal hash_width = 0;
    int last_row     = -1;

    for (const GraphNode* node : m_nodes) {
        hash_width = std::max(hash_width, font_metrics.horizontalAdvance(node->nodeData().hash));
        last_row   = std::max(last_row, node->nodeData().row);
    }

    m_metrics->hash_width = hash_width + (2 * GraphNode::padding);

    const qreal width      = std::max(mapToScene(viewport()->rect()).boundingRect().width(), GraphNode::minWidth);
    const qreal row_height = GraphNode::height + GraphNode::gap;

    for (GraphNode* node : m_nodes) {
        node->setWidth(width);
        node->setPos(0, (node->nodeData().row * row_height) + GraphNode::gap);
        node->update();
    }

    setSceneRect(0, 0, width, ((last_row + 1) * row_height) + GraphNode::gap);
}

void GraphWidget::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    relayout();
}

}
