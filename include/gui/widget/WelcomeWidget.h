#pragma once

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace gui::widget {

class WelcomeWidget : public QWidget {
    Q_OBJECT

public:
    WelcomeWidget(QWidget* parent = nullptr);

signals:
    void openCurrentDirectory();
    void openRepository();
    void loadSaveFile();

private:
    void setupUI();
    bool isValidGitRepository(const QString& path);
    void updateCurrentDirButton();

    QPushButton* m_open_curr_dir;
    QPushButton* m_open_repo;
    QPushButton* m_load_save;
    QLabel* m_title;
};

}
