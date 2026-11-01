#pragma once

#include "core/conflict/conflict.h"

#include <git2/types.h>
#include <memory>
#include <QColor>
#include <QGraphicsView>
#include <QString>

namespace gui::git {

struct GraphNodeData {
    using GraphNodeState = core::conflict::ConflictState;

    QString id;
    git_commit* commit = nullptr;
    int row            = -1;
    QString hash;
    QString message;
    GraphNodeState state = GraphNodeState::Clean;
    QString tooltip;
    QColor fill;

    [[nodiscard]] bool operator==(const GraphNodeData&) const = default;
};

class GraphNode;
struct GraphMetrics;

/**
 * @brief A column of nodes: [hash box][message], coloured by the state of the node.
 *
 * The widget only draws the data it is given and tells the owner which node the user picked.
 * It never changes the state of a node by itself.
 */
class GraphWidget : public QGraphicsView {
    Q_OBJECT

public:
    explicit GraphWidget(QWidget* parent = nullptr);
    ~GraphWidget() override;

    void setNodes(const QList<GraphNodeData>& nodes);
    void clearNodes();

    bool updateNode(const GraphNodeData& data);
    bool setNodeState(const QString& id, GraphNodeData::GraphNodeState state);

    [[nodiscard]] bool contains(const QString& id) const { return m_by_id.contains(id); }

    [[nodiscard]] const GraphNodeData* nodeData(const QString& id) const;

    [[nodiscard]] int count() const { return static_cast<int>(m_nodes.size()); }

    [[nodiscard]] QString currentId() const { return m_current_id; }

    bool setCurrentId(const QString& id);
    void clearCurrent();

signals:
    void currentChanged(const QString& id, const QString& previous_id);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void onFocusItemChanged(QGraphicsItem* now, QGraphicsItem* old, Qt::FocusReason reason);

    void destroyNodes();
    void markCurrent(GraphNode* node);
    void relayout();

    std::unique_ptr<GraphMetrics> m_metrics;
    std::vector<GraphNode*> m_nodes;
    QHash<QString, GraphNode*> m_by_id;
    QString m_current_id;

    bool m_updating = false;
};

}
