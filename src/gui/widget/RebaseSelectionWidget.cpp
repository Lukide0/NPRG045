#include "gui/widget/RebaseSelectionWidget.h"

#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStackedWidget>
#include <Qt>
#include <QVBoxLayout>
#include <QWidget>

#include <string>
#include <vector>

namespace gui::widget {

RebaseSelectionWidget::RebaseSelectionWidget(QWidget* parent)
    : QWidget(parent) {

    m_stack       = new QStackedWidget();
    m_back_btn    = new QPushButton();
    m_forward_btn = new QPushButton();
    m_forward_btn->setEnabled(false);
    m_message = new QLabel();

    m_message->setObjectName("rebase-selection-message");

    auto* navigation = new QHBoxLayout();
    navigation->addWidget(m_back_btn);
    navigation->addStretch(1);
    navigation->addWidget(m_forward_btn);

    auto* layout = new QVBoxLayout();
    layout->addWidget(m_stack);
    layout->addLayout(navigation);

    setLayout(layout);

    // branch page
    {
        auto* branch_page   = new QWidget();
        auto* branch_layout = new QVBoxLayout(branch_page);

        auto* branch_title = new QLabel("Select branch to rebase");
        branch_title->setProperty("class", "title");

        m_branches = new QComboBox(branch_page);

        branch_layout->addWidget(branch_title);
        branch_layout->addWidget(m_branches);
        branch_layout->addStretch(1);

        m_stack->addWidget(branch_page);
    }
    // commits page
    {
        auto* commits_page   = new QWidget();
        auto* commits_layout = new QVBoxLayout(commits_page);

        auto* commits_title = new QLabel("Select stating commit");
        commits_title->setProperty("class", "title");

        auto* commits_descrition
            = new QLabel("The list does NOT contain merge commits. The merge commits are dropped from todo list.");
        commits_descrition->setAlignment(Qt::AlignTop | Qt::AlignLeft);

        m_commits = new QListWidget(commits_page);
        m_commits->setSelectionMode(QAbstractItemView::SingleSelection);

        commits_layout->addWidget(commits_title);
        commits_layout->addWidget(commits_descrition);
        commits_layout->addWidget(m_message);
        commits_layout->addWidget(m_commits);

        m_stack->addWidget(commits_page);
    }

    updatePage(page_branch);

    connect(m_back_btn, &QPushButton::clicked, this, [this]() {
        const auto curr = m_stack->currentIndex();
        if (curr == page_branch) {
            emit cancelled();
            return;
        }

        updatePage(curr - 1);
    });

    connect(m_forward_btn, &QPushButton::clicked, this, [this]() {
        const auto curr = m_stack->currentIndex();
        if (curr == page_commits) {
            QListWidgetItem* item = m_commits->currentItem();
            if (item == nullptr) {
                return;
            }
            emit completed(item->data(Qt::UserRole).toString());
            return;
        } else if (curr == page_branch) {
            const int index = m_branches->currentIndex();

            if (index != m_branch_index) {
                m_branch_index = index;

                emit branchSelectionChanged(selectedBranch());
            }
        }

        updatePage(curr + 1);
    });

    connect(m_commits, &QListWidget::itemSelectionChanged, this, [this]() {
        m_forward_btn->setEnabled(m_commits->currentRow() >= 0);
    });

    connect(m_branches, &QComboBox::currentIndexChanged, this, [this]() {
        m_forward_btn->setEnabled(m_branches->currentIndex() >= 0);
    });
}

void RebaseSelectionWidget::updatePage(int page) {
    page = std::min(pages - 1, page);
    if (page == page_branch) {
        m_back_btn->setText("Cancel");
        m_forward_btn->setText("Next");
        m_forward_btn->setEnabled(m_branches->currentIndex() >= 0);

    } else {
        m_back_btn->setText("Back");
        m_forward_btn->setText("Rebase");
        m_forward_btn->setEnabled(m_commits->currentRow() >= 0);
    }

    m_stack->setCurrentIndex(page);
}

void RebaseSelectionWidget::setBranches(std::vector<std::string>&& branches) {
    m_branches->clear();
    m_branch_names = std::move(branches);

    for (const auto& branch : m_branch_names) {
        auto name = QString::fromStdString(branch);
        m_branches->addItem(name);
    }
}

void RebaseSelectionWidget::addCommit(const std::string& name, const std::string& id) {
    auto* list_item = new QListWidgetItem(QString::fromStdString(name));
    list_item->setData(Qt::UserRole, QString::fromStdString(id));

    m_commits->addItem(list_item);
}

void RebaseSelectionWidget::clearCommits() { m_commits->clear(); }

}
