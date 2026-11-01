#include "gui/git/GraphPresenter.h"

#include "app/rebase/RebasePlanModel.h"
#include "core/conflict/conflict.h"

#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QList>
#include <QMetaObject>
#include <QModelIndex>
#include <QString>

namespace gui::git {

using Model = app::rebase::RebasePlanModel;

QString first_line(const QString& text) { return text.section('\n', 0, 0); }

// The roles that change what the result graph shows.
bool affects_graph(const QList<int>& roles) {
    if (roles.isEmpty()) {
        return true;
    }

    for (int role : roles) {
        switch (role) {
        case Model::TypeRole:
        case Model::MessageRole:
        case Model::ConflictStateRole:
        case Model::CommitIdRole:
        case Model::HashRole:
        case Model::SummaryRole:
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            return true;
        default:
            break;
        }
    }

    return false;
}

ResultGraph build_result_graph(const app::rebase::RebasePlanModel& plan, const GraphNodeData& root) {
    using core::action::ActionType;

    ResultGraph result;

    GraphNodeData first = root;
    first.row           = 0;
    first.state         = GraphNodeData::GraphNodeState::Clean;
    result.nodes.push_back(std::move(first));

    const int count = plan.rowCount();
    result.row_node.reserve(count);

    for (int row = 0; row < count; ++row) {
        const QModelIndex index = plan.index(row, 0);
        const auto* action      = plan.actionAt(row);

        const auto type  = static_cast<ActionType>(index.data(Model::TypeRole).toInt());
        const auto state = static_cast<core::conflict::ConflictState>(index.data(Model::ConflictStateRole).toInt());

        switch (type) {
        case ActionType::DROP:
            result.row_node.push_back(QString());
            break;

        case ActionType::FIXUP:
        case ActionType::SQUASH: {
            GraphNodeData& last = result.nodes.back();
            if (last.state == GraphNodeData::GraphNodeState::Clean) {
                last.state = state;
            }

            result.row_node.push_back(last.id);
            break;
        }

        case ActionType::PICK:
        case ActionType::REWORD:
        case ActionType::EDIT: {
            GraphNodeData node;

            node.id     = index.data(Model::CommitIdRole).toString();
            node.row    = static_cast<int>(result.nodes.size());
            node.hash   = index.data(Model::HashRole).toString();
            node.state  = state;
            node.commit = action != nullptr ? action->get_commit() : nullptr;

            // a reword shows its new message
            const QString message = index.data(Model::MessageRole).toString();
            node.message          = first_line(message.isEmpty() ? index.data(Model::SummaryRole).toString() : message);

            node.tooltip = index.data(Qt::ToolTipRole).toString();

            result.node_row.insert(node.id, row);
            result.row_node.push_back(node.id);
            result.nodes.push_back(std::move(node));
            break;
        }
        }
    }

    return result;
}

GraphPresenter::GraphPresenter(GraphWidget* original_graph, GraphWidget* result_graph, QObject* parent)
    : QObject(parent)
    , m_original(original_graph)
    , m_result(result_graph) {

    connect(original_graph, &GraphWidget::currentChanged, this, [this](const QString& id) { onOriginalPicked(id); });
    connect(result_graph, &GraphWidget::currentChanged, this, [this](const QString& id) { onResultPicked(id); });
}

void GraphPresenter::setPlan(app::rebase::RebasePlanModel* plan, QItemSelectionModel* selection) {
    for (const auto& connection : m_connections) {
        QObject::disconnect(connection);
    }

    m_connections.clear();

    m_plan      = plan;
    m_selection = selection;

    m_connections.push_back(connect(plan, &QAbstractItemModel::rowsInserted, this, &GraphPresenter::scheduleRefresh));
    m_connections.push_back(connect(plan, &QAbstractItemModel::rowsRemoved, this, &GraphPresenter::scheduleRefresh));
    m_connections.push_back(connect(plan, &QAbstractItemModel::rowsMoved, this, &GraphPresenter::scheduleRefresh));
    m_connections.push_back(connect(plan, &QAbstractItemModel::modelReset, this, &GraphPresenter::scheduleRefresh));
    m_connections.push_back(connect(
        plan,
        &QAbstractItemModel::dataChanged,
        this,
        [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
            if (affects_graph(roles)) {
                scheduleRefresh();
            }
        }
    ));

    m_connections.push_back(connect(selection, &QItemSelectionModel::currentChanged, this, [this] {
        if (!m_pending) {
            syncCurrent();
        }
    }));
}

void GraphPresenter::setOriginal(QList<GraphNodeData> history) {
    m_root = history.empty() ? GraphNodeData { } : history.first();
    m_original->setNodes(history);

    m_last_result.clear();
    scheduleRefresh();
}

void GraphPresenter::scheduleRefresh() {
    if (m_pending) {
        return;
    }

    m_pending = true;
    QMetaObject::invokeMethod(this, &GraphPresenter::refresh, Qt::QueuedConnection);
}

void GraphPresenter::refresh() {
    m_pending = false;

    if (m_root.row == -1) {

        m_result->clearNodes();
        m_last_result.clear();
    }

    if (m_root.id.isEmpty() || m_plan == nullptr) {
        m_mapping = { };
        return;
    }

    m_mapping = build_result_graph(*m_plan, m_root);

    if (m_mapping.nodes != m_last_result) {
        m_result->setNodes(m_mapping.nodes);
        m_last_result = m_mapping.nodes;
    }

    syncCurrent();
}

void GraphPresenter::syncCurrent() {
    if (m_selection == nullptr) {
        m_original->clearCurrent();
        m_result->clearCurrent();
        return;
    }

    const QModelIndex index = m_selection->currentIndex();

    if (!index.isValid()) {
        m_original->clearCurrent();
        m_result->clearCurrent();
        return;
    }

    const QString node = index.row() < m_mapping.row_node.size() ? m_mapping.row_node.at(index.row()) : QString();
    if (node.isEmpty() || !m_result->setCurrentId(node)) {
        m_result->clearCurrent();
    }

    if (!m_original->setCurrentId(index.data(Model::CommitIdRole).toString())) {
        m_original->clearCurrent();
    }
}

void GraphPresenter::onOriginalPicked(const QString& id) { emit commitPicked(id); }

void GraphPresenter::onResultPicked(const QString& id) {
    auto it = m_mapping.node_row.constFind(id);

    if (it != m_mapping.node_row.constEnd()) {
        emit rowRequested(it.value());
        return;
    }

    emit commitPicked(id);
}
}
