#include "gui/widget/CommitViewWidget.h"
#include "core/git/diff.h"
#include "core/git/GitGraph.h"
#include "core/git/types.h"
#include "gui/clear_layout.h"
#include "gui/widget/graph/Node.h"

#include <cstdlib>
#include <format>
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

void CommitViewWidget::createRows() {
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

    const char* summary          = git_commit_summary(commit);
    const char* desc             = git_commit_body(commit);
    const git_signature* autor   = git_commit_author(commit);
    const git_time_t commit_time = git_commit_time(commit);
    const int commit_time_offset = git_commit_time_offset(commit);

    std::stringstream ss;
    {
        std::time_t time  = commit_time;
        std::tm* timeInfo = std::gmtime(&time);

        int offset_hours   = std::abs(commit_time_offset) / 60;
        int offset_minutes = std::abs(commit_time_offset) % 60;

        // clang-format off
        ss << std::put_time(timeInfo, "%Y-%m-%dT%H:%M:%S")
           << ((commit_time_offset < 0) ? '-' : '+')
           << std::format("{:02}:{:02}", offset_hours, offset_minutes);
        // clang-format on
    }
    auto time_str = ss.str();

    m_info_layout->addRow("Hash:", new QLabel(hash.c_str()));
    m_info_layout->addRow("Author:", new QLabel(autor->name));
    m_info_layout->addRow("Date:", new QLabel(time_str.c_str()));
    m_info_layout->addRow("Summary:", new QLabel(summary));
    m_info_layout->addRow("Description:", new QLabel(desc));
}

void CommitViewWidget::prepareDiff() {
    m_changes->clear();

    if (m_node == nullptr) {
        return;
    }

    Node* parentNode = m_node->getParentNode();

    git_commit* curr_commit   = m_node->getCommit();
    git_commit* parent_commit = (parentNode != nullptr) ? parentNode->getCommit() : nullptr;

    diff_result_t res = prepare_diff(parent_commit, curr_commit);
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

    git_diff_foreach(
        res.diff.diff,
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
