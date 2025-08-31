#include "gui/widget/WelcomeWidget.h"
#include <git2/repository.h>
#include <git2/types.h>

namespace gui::widget {

WelcomeWidget::WelcomeWidget(QWidget* parent)
    : QWidget(parent) {

    setupUI();
    updateCurrentDirButton();

    connect(m_open_curr_dir, &QPushButton::clicked, this, [this]() { emit openCurrentDirectory(); });
    connect(m_open_repo, &QPushButton::clicked, this, [this]() { emit openRepository(); });
    connect(m_load_save, &QPushButton::clicked, this, [this]() { emit loadSaveFile(); });
}

void WelcomeWidget::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(20);
    layout->setContentsMargins(40, 40, 40, 40);

    m_title = new QLabel("Git Shuffle");
    m_title->setAlignment(Qt::AlignCenter);

    QFont title_font = m_title->font();
    title_font.setBold(true);
    title_font.setPointSize(title_font.pointSize() + 8);
    m_title->setFont(title_font);

    auto* subtitle = new QLabel("Choose how to start:");
    subtitle->setAlignment(Qt::AlignCenter);

    auto* button_layout = new QVBoxLayout();
    button_layout->setSpacing(15);

    m_open_curr_dir = new QPushButton("Open Current Working Directory");
    m_open_curr_dir->setMinimumHeight(50);

    m_open_repo = new QPushButton("Browse for Git Repository");
    m_open_repo->setMinimumHeight(50);

    m_load_save = new QPushButton("Load save file");
    m_load_save->setMinimumHeight(50);

    button_layout->addWidget(m_open_curr_dir);
    button_layout->addWidget(m_open_repo);
    button_layout->addWidget(m_load_save);

    layout->addWidget(m_title);
    layout->addWidget(subtitle);
    layout->addLayout(button_layout);

    setFixedSize(400, 350);
}

bool WelcomeWidget::isValidGitRepository(const QString& path) {
    git_repository* repo = nullptr;
    int error            = git_repository_open(&repo, path.toUtf8().constData());

    if (repo != nullptr) {
        git_repository_free(repo);
        return (error == 0);
    }

    return false;
}

void WelcomeWidget::updateCurrentDirButton() {
    QString curr_path  = QDir::currentPath();
    bool is_valid_repo = isValidGitRepository(curr_path);

    m_open_curr_dir->setEnabled(is_valid_repo);

    if (is_valid_repo) {
        m_open_curr_dir->setToolTip(QString("Open repository at: %1").arg(curr_path));
    } else {
        m_open_curr_dir->setToolTip(QString("Current directory is not a git repository: %1").arg(curr_path));
    }
}

}
