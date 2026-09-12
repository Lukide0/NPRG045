#pragma once

#include <git2/oid.h>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>
#include <string>
#include <vector>

namespace gui::widget {

class RebaseSelectionWidget : public QWidget {
    Q_OBJECT

public:
    RebaseSelectionWidget(QWidget* parent = nullptr);

    void setBranches(std::vector<std::string>&& branches);

    void addCommit(const std::string& name, const std::string& id);
    void clearCommits();

    void setMessage(const QString& msg) {
        m_message->setText(msg);
        m_message->show();
    }

    void clearMessage() { m_message->hide(); }

    [[nodiscard]] const std::string& selectedBranch() const { return m_branch_names[m_branch_index]; }

signals:
    void cancelled();
    void branchSelectionChanged(const std::string& branch);

    void completed(QString commit_id);

private:
    static constexpr int page_branch  = 0;
    static constexpr int page_commits = 1;
    static constexpr int pages        = 2;

    std::vector<std::string> m_branch_names;
    int m_branch_index = -1;

    QStackedWidget* m_stack;

    QComboBox* m_branches;
    QListWidget* m_commits;
    QLabel* m_message;

    QPushButton* m_back_btn;
    QPushButton* m_forward_btn;

    void updatePage(int page);
};

}
