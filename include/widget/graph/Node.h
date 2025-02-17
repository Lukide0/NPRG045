#pragma once

#include "git/types.h"
#include <git2/commit.h>
#include <git2/types.h>
#include <iostream>
#include <QColor>
#include <QGraphicsItem>
#include <QPainterPath>
#include <string>

class GraphWidget;

class Node : public QGraphicsItem {
public:
    Node(GraphWidget* graph)
        : m_graph(graph) {
        setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
    }

    static constexpr int Type = UserType + 1;

    [[nodiscard]] int type() const override { return Type; }

    void setCommit(git_commit* commit) {
        auto id  = format_oid(commit);
        m_commit = commit;
        m_hash   = id.data();
        m_msg    = git_commit_summary(commit);
    }

    git_commit* getCommit() { return m_commit; }

    Node* getParentNode() { return m_parent; }

    void setParentNode(Node* parent) { m_parent = parent; }

    void setGitTree(git_tree* tree) { m_tree = tree; }

    void setGitTreeOwnership(git_tree* tree) {
        m_tree_owner = tree;
        m_tree = tree;
    }

    git_tree* clearGitTreeOwnership()
    {
        return m_tree_owner.release();
    }

    void setConflict(bool conflict) { m_has_conflict = conflict; }

    [[nodiscard]] bool hasConflict() const { return m_has_conflict; }

    [[nodiscard]] git_tree* getGitTree() const { return m_tree; }

    void setFill(const QColor& color);

    [[nodiscard]] QRectF boundingRect() const override;
    [[nodiscard]] QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    GraphWidget* m_graph;
    QColor m_fill = Qt::white;
    std::string m_hash;
    std::string m_msg;
    git_commit* m_commit;
    git_tree* m_tree    = nullptr;
    git_tree_t m_tree_owner;
    Node* m_parent      = nullptr;
    bool m_has_conflict = false;
};
