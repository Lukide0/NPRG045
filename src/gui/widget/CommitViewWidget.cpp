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

#include <QHBoxLayout>
#include <QMessageBox>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

CommitViewWidget::CommitViewWidget(DiffWidget* diff)
    : m_diff(diff) {

    m_layout = new QHBoxLayout();
    setLayout(m_layout);

    m_info_layout = new QFormLayout();
    m_layout->addLayout(m_info_layout, 2);

    m_changes = new NamedListWidget("Changes");
    m_layout->addWidget(m_changes);

    createRows();

    auto* list = m_changes->getList();

    connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        auto index = item->data(Qt::UserRole).toInt();
        auto* file = m_diff->getDiffFile(index);

        m_diff->ensureEditorVisible(file);
    });
}

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

    auto* list        = m_changes->getList();
    const auto& diffs = m_diff->getDiffs();

    for (std::size_t i = 0; i < diffs.size(); ++i) {
        const auto& file_diff = diffs[i];
        QString item_text;
        switch (file_diff.state) {
        case diff_files_t::State::UNMODIFIED:
        case diff_files_t::State::UNREADABLE:
        case diff_files_t::State::UNTRACKED:
        case diff_files_t::State::CONFLICTED:
        case diff_files_t::State::IGNORED:
        case diff_files_t::State::TYPECHANGE:
            UNEXPECTED("Unexpected state");

        case diff_files_t::State::ADDED:
            item_text += "New ";
            item_text += QString::fromStdString(file_diff.new_file.path);
            break;
        case diff_files_t::State::DELETED:
            item_text += "Deleted ";
            item_text += QString::fromStdString(file_diff.old_file.path);
            break;
        case diff_files_t::State::MODIFIED:
            item_text += "Modified ";
            item_text += QString::fromStdString(file_diff.new_file.path);
            break;
        case diff_files_t::State::RENAMED:
            item_text += "Renamed ";
            item_text += QString::fromStdString(file_diff.old_file.path);
            item_text += " -> ";
            item_text += QString::fromStdString(file_diff.new_file.path);
            break;
        case diff_files_t::State::COPIED:
            item_text += "Copied ";
            item_text += QString::fromStdString(file_diff.old_file.path);
            item_text += " -> ";
            item_text += QString::fromStdString(file_diff.new_file.path);
            break;
        }

        auto* item = new QListWidgetItem(item_text);
        item->setData(Qt::UserRole, QVariant::fromValue<int>(i));
        list->addItem(item);
    }
}
