#include "gui/widget/CommitViewWidget.h"
#include "core/git/GitGraph.h"
#include "core/git/types.h"
#include "gui/clear_layout.h"
#include "gui/widget/graph/Node.h"

#include <git2/commit.h>
#include <git2/diff.h>
#include <git2/tree.h>
#include <git2/types.h>

#include <QMessageBox>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

void CommitViewWidget::create_rows() {
    clear_layout(m_info_layout);

    if (m_node == nullptr) {
        m_info_layout->addRow("Hash:", new QLabel(""));
        m_info_layout->addRow("Author:", new QLabel(""));
        m_info_layout->addRow("Date:", new QLabel(""));
        m_info_layout->addRow("Summary:", new QLabel(""));
        m_info_layout->addRow("Description:", new QLabel(""));
        return;
    }

    git_commit* commit = m_node->getCommit();

    std::string hash = GitGraph<Node*>::get_commit_id(commit);

    const char* summary        = git_commit_summary(commit);
    const char* desc           = git_commit_body(commit);
    const git_signature* autor = git_commit_author(commit);

    std::stringstream ss;
    {
        std::chrono::seconds seconds { autor->when.time };
        std::time_t time = seconds.count();
        std::tm* t       = std::gmtime(&time);

        ss << std::put_time(t, "%x %X");
    }
    auto time_str = ss.str();

    m_info_layout->addRow("Hash:", new QLabel(hash.c_str()));
    m_info_layout->addRow("Author:", new QLabel(autor->name));
    m_info_layout->addRow("Date:", new QLabel(time_str.c_str()));
    m_info_layout->addRow("Summary:", new QLabel(summary));
    m_info_layout->addRow("Description:", new QLabel(desc));
}

void CommitViewWidget::prepare_diff() {
    m_changes->clear();

    if (m_node == nullptr) {
        return;
    }

    Node* parentNode = m_node->getParentNode();
    git_tree_t curr_tree;
    git_tree_t parent_tree;

    if (parentNode != nullptr) {
        git_commit* curr   = m_node->getCommit();
        git_commit* parent = parentNode->getCommit();

        if (git_commit_tree(&curr_tree.tree, curr) != 0 || git_commit_tree(&parent_tree.tree, parent) != 0) {
            QMessageBox::critical(this, "Commit diff error", "Failed to retrieve tree from commit");
            return;
        }
    } else {
        parent_tree.tree = nullptr;
        git_commit* curr = m_node->getCommit();

        if (git_commit_tree(&curr_tree.tree, curr) != 0) {
            QMessageBox::critical(this, "Commit diff error", "Failed to retrieve tree from commit");
            return;
        }
    }

    git_diff_t diff;
    git_repository* repo = git_tree_owner(curr_tree.tree);
    if (git_diff_tree_to_tree(&diff.diff, repo, parent_tree.tree, curr_tree.tree, nullptr) != 0) {
        QMessageBox::critical(this, "Commit diff error", "Failed to create diff");
        return;
    }

    git_diff_foreach(
        diff.diff,
        [](const git_diff_delta* delta, float /*unused*/, void* list_raw) -> int {
            auto* list = reinterpret_cast<QListWidget*>(list_raw);

            QString str;
            switch (delta->status) {
            case GIT_DELTA_UNMODIFIED:
            case GIT_DELTA_UNREADABLE:
            case GIT_DELTA_CONFLICTED:
            case GIT_DELTA_UNTRACKED:
            case GIT_DELTA_IGNORED:
                break;
            case GIT_DELTA_ADDED:
                str += "Added ";
                str += delta->new_file.path;
                break;
            case GIT_DELTA_DELETED:
                str += "Deleted ";
                str += delta->old_file.path;
                break;
            case GIT_DELTA_MODIFIED:
                str += "Modified ";
                str += delta->new_file.path;
                break;
            case GIT_DELTA_RENAMED:
                str += "Renamed ";
                str += delta->old_file.path;
                str += " -> ";
                str += delta->new_file.path;
                break;
            case GIT_DELTA_COPIED:
                str += "Copied ";
                str += delta->old_file.path;
                str += " -> ";
                str += delta->new_file.path;
                break;
            case GIT_DELTA_TYPECHANGE:
                str += "Typechange ";
                str += delta->old_file.path;
                break;
            }

            list->addItem(str);

            return 0;
        },
        nullptr,
        nullptr,
        nullptr,
        m_changes->getList()
    );
}
