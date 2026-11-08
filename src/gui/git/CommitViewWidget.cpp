#include "gui/git/CommitViewWidget.h"

#include "App.h"
#include "core/action/Action.h"
#include "core/git/diff.h"
#include "core/git/GitGraph.h"
#include "core/git/types.h"
#include "gui/git/CommitMessageWidget.h"
#include "gui/git/DiffWidget.h"
#include "gui/widget/NamedListWidget.h"
#include "gui/widget/SLabel.h"

#include <git2/commit.h>
#include <git2/diff.h>
#include <git2/oid.h>
#include <git2/tree.h>
#include <git2/types.h>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QString>
#include <Qt>
#include <QVariant>

#include <cstdlib>
#include <ctime>
#include <format>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace gui::git {

using core::action::Action;
using core::git::diff_files_t;
using core::git::diff_line_t;
using core::git::GitGraph;

CommitViewWidget::CommitViewWidget(DiffWidget* diff, QWidget* parent)
    : QWidget(parent)
    , m_diff(diff) {
    m_layout = new QHBoxLayout();
    setLayout(m_layout);

    m_info_layout = new QFormLayout();
    m_layout->addLayout(m_info_layout, 2);

    m_changes = new NamedListWidget("Changes");
    m_layout->addWidget(m_changes);

    m_labels.hash    = new SLabel();
    m_labels.autor   = new SLabel();
    m_labels.date    = new SLabel();
    m_message_widget = new CommitMessageWidget();

    m_info_layout->addRow("Hash:", m_labels.hash);
    m_info_layout->addRow("Author:", m_labels.autor);
    m_info_layout->addRow("Date:", m_labels.date);
    m_info_layout->addRow("Message:", m_message_widget);

    auto* list = m_changes->getList();
    connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        auto index = item->data(Qt::UserRole).toInt();
        auto* file = m_diff->getDiffFile(index);

        if (file == nullptr) {
            return;
        }

        m_diff->ensureEditorVisible(file);
    });

    connect(m_message_widget, &CommitMessageWidget::commitMessageChanged, this, [this](const QString& msg) {
        if (m_step == nullptr || msg.isEmpty()) {
            return;
        }

        emit commitMessageChanged(m_step->index, msg);
    });
}

void CommitViewWidget::setCurrentStep(core::rebase::RebaseAnalysisConstPtr analysis, int row) {
    m_step   = nullptr;
    m_commit = nullptr;

    if (analysis == nullptr || row < 0 || static_cast<std::size_t>(row) >= analysis->steps.size()) {
        updateCommit();
        return;
    }

    const auto& step = analysis->steps[row];
    m_step           = &step;
    m_commit         = step.action->get_commit();

    updateCommit();
}

void CommitViewWidget::setCurrentCommit(git_commit* commit) {
    m_step   = nullptr;
    m_commit = commit;

    updateCommit();
}

void CommitViewWidget::createInfoLayout() {
    if (m_commit == nullptr) {
        m_labels.clear();
        m_message_widget->clearText();
        return;
    }

    if (m_step == nullptr || !m_step->action->can_edit_msg()) {
        m_message_widget->disableEdit();
    } else {
        m_message_widget->enableEdit();
    }

    if (m_step == nullptr) {
        const char* msg = git_commit_message(m_commit);
        m_message_widget->setText(msg);

    } else {
        const auto* sesion           = App::rebaseSession();
        std::string_view comment_str = "#";
        if (sesion != nullptr) {
            comment_str = sesion->repo().comment_string();
        }

        m_message_widget->setAction(m_step->action, comment_str);
    }

    std::string hash = core::git::format_oid_to_str<core::git::OID_SIZE>(git_commit_id(m_commit));

    const git_signature* autor   = git_commit_author(m_commit);
    const git_time_t commit_time = git_commit_time(m_commit);
    const int commit_time_offset = git_commit_time_offset(m_commit);

    std::stringstream ss;
    {
        std::time_t time  = commit_time;
        std::tm* timeInfo = std::gmtime(&time);

        int offset_hours   = std::abs(commit_time_offset) / 60;
        int offset_minutes = std::abs(commit_time_offset) % 60;

        // clang-format off
        ss << std::put_time(timeInfo, "%Y-%m-%d %H:%M:%S")
           << ((commit_time_offset < 0) ? '-' : '+')
           << std::format("{:02}:{:02}", offset_hours, offset_minutes);
        // clang-format on
    }

    m_labels.hash->setText(QString::fromStdString(hash));
    m_labels.autor->setText(autor->name);
    m_labels.date->setText(QString::fromStdString(ss.str()));
}

void CommitViewWidget::createDiffList() {
    m_changes->clear();

    if (m_commit == nullptr) {
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
            std::unreachable();

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
        case diff_files_t::State::MOVED:
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
        item->setData(Qt::UserRole, QVariant::fromValue<int>(static_cast<int>(i)));
        list->addItem(item);
    }
}

void CommitViewWidget::updateCommit() {
    createInfoLayout();
    createDiffList();
}
}
