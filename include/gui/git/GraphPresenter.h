#pragma once

#include "app/rebase/RebasePlanModel.h"
#include "gui/git/Graph.h"
#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QObject>
#include <vector>

namespace gui::git {

struct ResultGraph {
    QList<GraphNodeData> nodes;
    QStringList row_node;
    QHash<QString, int> node_row;
};

/**
 * @brief Feeds the two graphs (original sequence and result) and keeps them in step with the list.
 */
class GraphPresenter : public QObject {
    Q_OBJECT

public:
    GraphPresenter(GraphWidget* original_graph, GraphWidget* result_graph, QObject* parent = nullptr);

    void setPlan(app::rebase::RebasePlanModel* plan, QItemSelectionModel* selection);

    void setOriginal(QList<GraphNodeData> history);

signals:
    void rowRequested(int row);
    void commitPicked(const QString& commit_id);

private:
    void scheduleRefresh();
    void refresh();
    void syncCurrent();

    void onOriginalPicked(const QString& id);
    void onResultPicked(const QString& id);

    app::rebase::RebasePlanModel* m_plan = nullptr;
    QItemSelectionModel* m_selection     = nullptr;
    GraphWidget* m_original;
    GraphWidget* m_result;

    GraphNodeData m_root;
    QList<GraphNodeData> m_last_result;
    ResultGraph m_mapping;

    bool m_pending = false;

    QList<QMetaObject::Connection> m_connections;
};

}
