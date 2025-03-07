#include "gui/widget/DiffWidget.h"
#include "core/git/diff.h"
#include "gui/widget/graph/Node.h"

#include <git2/diff.h>
#include <git2/types.h>
#include <QMessageBox>
#include <QTextDocument>

DiffWidget::DiffWidget(QWidget* parent)
    : QWidget(parent) {
    m_layout = new QVBoxLayout();
    setLayout(m_layout);
}

void DiffWidget::update(Node* node) {
    m_node = node;

    if (m_node == nullptr) {
        return;
    }

    git_commit* commit        = node->getCommit();
    git_commit* parent_commit = nullptr;

    if (Node* parent_node = node->getParentNode()) {
        parent_commit = parent_node->getCommit();
    }

    diff_result_t res = prepare_diff(parent_commit, commit);
    switch (res.state) {
    case diff_result_t::FAILED_TO_RETRIEVE_TREE:
        QMessageBox::critical(this, "Commit diff error", "Failed to retrieve tree from commit");
        return;

    case diff_result_t::FAILED_TO_CREATE_DIFF:
        QMessageBox::critical(this, "Commit diff error", "Failed to create diff");
        return;
    case diff_result_t::OK:
        break;
    }

    m_diffs = create_diff(res.diff.diff);
}
