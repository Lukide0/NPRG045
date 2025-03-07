#pragma once

#include "core/git/diff.h"
#include "gui/widget/graph/Node.h"
#include <QVBoxLayout>
#include <QWidget>
#include <vector>

class DiffWidget : public QWidget {
public:
    DiffWidget(QWidget* parent = nullptr);

    void update(Node* node);

private:
    Node* m_node = nullptr;
    std::vector<diff_files_t> m_diffs;
    QVBoxLayout* m_layout;
};
