#pragma once

#include "action/Action.h"
#include "core/git/types.h"

#include <git2/commit.h>
#include <git2/types.h>

#include <QColor>
#include <QGraphicsItem>
#include <QPainterPath>

#include <string>

namespace gui::widget {

class GraphWidget;

class Node : public QGraphicsItem {
public:
    static constexpr qreal MIN_WIDTH = 300;
    static constexpr qreal HEIGHT    = 20;

    using ConflictStatus = core::conflict::ConflictStatus;
    using Action         = action::Action;

    Node(GraphWidget* graph)
        : m_graph(graph) {
        setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
    }

    ~Node() override = default;

    static constexpr int Type = UserType + 1;

    [[nodiscard]] int type() const override { return Type; }

    void setWidth(qreal width) { m_width = std::max(MIN_WIDTH, width); }

    void setCommit(git_commit* commit) {
        auto id       = core::git::format_oid(commit);
        m_commit      = commit;
        m_commit_hash = id.data();
        m_commit_msg  = git_commit_summary(commit);
    }

    void setMessage(const std::string& str) { m_commit_msg = str; }

    void setAction(Action* action) { m_action = action; }

    Action* getAction() { return m_action; }

    git_commit* getCommit() { return m_commit; }

    [[nodiscard]] const git_commit* getCommit() const { return m_commit; }

    [[nodiscard]] const git_oid* getCommitId() const { return git_commit_id(m_commit); }

    Node* getParentNode() { return m_parent; }

    void setParentNode(Node* parent) { m_parent = parent; }

    void setConflict(ConflictStatus conflict) { m_conflict = conflict; }

    void updateConflict(ConflictStatus conflict);

    [[nodiscard]] bool hasConflict() const { return m_conflict == ConflictStatus::HAS_CONFLICT; }

    void setFill(const QColor& color);

    [[nodiscard]] QRectF boundingRect() const override;
    [[nodiscard]] QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    GraphWidget* m_graph;
    QColor m_fill = Qt::white;

    qreal m_width = MIN_WIDTH;

    git_commit* m_commit;
    std::string m_commit_hash;
    std::string m_commit_msg;

    Action* m_action = nullptr;

    Node* m_parent = nullptr;

    ConflictStatus m_conflict = ConflictStatus::NO_CONFLICT;
};

}
