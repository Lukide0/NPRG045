#pragma once

#include "core/rebase/RebaseAnalysis.h"
#include "gui/widget/CommitMessageWidget.h"
#include "gui/widget/DiffWidget.h"
#include "gui/widget/NamedListWidget.h"

#include <git2/commit.h>
#include <git2/types.h>

#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QWidget>

namespace gui::widget {

class CommitViewWidget : public QWidget {
    Q_OBJECT

public:
    CommitViewWidget(DiffWidget* diff, QWidget* parent = nullptr);

public slots:
    void setCurrentStep(core::rebase::RebaseAnalysisConstPtr analysis, int row);
    void setCurrentCommit(git_commit* commit);

signals:
    void commitMessageChanged(std::size_t row, const QString& msg);

private:
    QHBoxLayout* m_layout;
    QFormLayout* m_info_layout;
    NamedListWidget* m_changes;
    CommitMessageWidget* m_message_widget;

    struct {
        QLabel* hash;
        QLabel* autor;
        QLabel* date;

        void clear() {
            hash->clear();
            autor->clear();
            date->clear();
        }
    } m_labels;

    DiffWidget* m_diff;
    const core::rebase::StepResult* m_step = nullptr;
    const git_commit* m_commit             = nullptr;

    void createInfoLayout();
    void createDiffList();
    void updateCommit();
};

}
