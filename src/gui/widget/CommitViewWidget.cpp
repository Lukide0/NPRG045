#include "gui/widget/CommitViewWidget.h"
#include "core/git/diff.h"
#include "core/git/GitGraph.h"
#include "core/git/types.h"
#include "core/utils/unexpected.h"
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

    auto* list = m_changes->getList();

    for (const auto& file_diff : m_diff->getDiffs()) {
        QString item;
        switch (file_diff.state) {
        case diff_files_t::State::UNMODIFIED:
        case diff_files_t::State::UNREADABLE:
        case diff_files_t::State::UNTRACKED:
        case diff_files_t::State::CONFLICTED:
        case diff_files_t::State::IGNORED:
        case diff_files_t::State::TYPECHANGE:
            UNEXPECTED("Unexpected state");

        case diff_files_t::State::ADDED:
            item += "New ";
            item += file_diff.new_file.path;
            break;
        case diff_files_t::State::DELETED:
            item += "Deleted ";
            item += file_diff.old_file.path;
            break;
        case diff_files_t::State::MODIFIED:
            item += "Modified ";
            item += file_diff.new_file.path;
            break;
        case diff_files_t::State::RENAMED:
            item += "Renamed ";
            item += file_diff.old_file.path;
            item += " -> ";
            item += file_diff.new_file.path;
            break;
        case diff_files_t::State::COPIED:
            item += "Copied ";
            item += file_diff.old_file.path;
            item += " -> ";
            item += file_diff.new_file.path;
            break;
        }

        list->addItem(item);
    }
}
