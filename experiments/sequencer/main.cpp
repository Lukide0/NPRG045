#include "ListItem.h"
#include "parser.h"
#include "utils.h"

#include <cassert>
#include <cctype>
#include <fstream>
#include <git2.h>
#include <git2/commit.h>
#include <git2/errors.h>
#include <git2/global.h>
#include <git2/repository.h>
#include <git2/types.h>
#include <iostream>
#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QString>
#include <string>
#include <unordered_map>
#include <vector>

constexpr auto* TODO_FILE_PATH = ".git/rebase-merge/git-rebase-todo";
constexpr auto* HEAD_FILE      = ".git/rebase-merge/orig-head";
constexpr auto* ONTO_FILE      = ".git/rebase-merge/onto";

void clear_layout(QLayout* layout) {
    if (layout == nullptr) {
        return;
    }

    while (auto* child = layout->takeAt(0)) {
        if (auto* w = child->widget()) {
            w->deleteLater();
        } else if (auto* l = child->layout()) {
            clear_layout(l);
        }

        delete child;
    }
}

class MainWindow : public QMainWindow {
public:
    MainWindow() {
        git_libgit2_init();

        // TOP BAR ------------------------------------------------------------
        auto* menu = new QMenuBar(this);
        setMenuBar(menu);

        auto* repo = menu->addMenu("Repo");

        auto* repo_open = new QAction(QIcon::fromTheme(QIcon::ThemeIcon::FolderOpen), "Open", this);
        repo_open->setStatusTip("Open a repo");
        connect(repo_open, &QAction::triggered, this, &MainWindow::open_repo);

        repo->addAction(repo_open);

        // MAIN ---------------------------------------------------------------
        auto* main = new QWidget(this);
        setCentralWidget(main);

        m_layout = new QHBoxLayout(main);

        m_help_label = new QLabel("Open a repo with rebase in progress", main);
        m_layout->addWidget(m_help_label);
    }

    void open_repo() {
        auto folder = QFileDialog::getExistingDirectory(
            this, "Select Repo folder", QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

        if (folder.isEmpty()) {
            return;
        }

        auto path = folder.toStdString();

        if (git_repository_open(&m_repo, path.c_str()) != 0) {

            git_repository_free(m_repo);
            m_repo = nullptr;

            const auto* err = git_error_last();
            QMessageBox::critical(this, "Repo Error", err->message);
            return;
        }

        m_repo_path = path;

        show_rebase();
    }

    ~MainWindow() override {
        git_repository_free(m_repo);
        git_libgit2_shutdown();
    }

private:
    std::string m_repo_path;
    QWidget* m_help_label;
    QHBoxLayout* m_layout;
    git_repository* m_repo = nullptr;
    ListItem* m_last_item  = nullptr;

    void show_rebase() {
        std::string head;
        std::string onto;

        {
            auto head_file = std::ifstream(m_repo_path + '/' + HEAD_FILE);
            auto onto_file = std::ifstream(m_repo_path + '/' + ONTO_FILE);

            if (!head_file.good() || !onto_file.good()) {
                QMessageBox::critical(this, "Rebase Error", "Could not find rebase files");
                return;
            }

            std::getline(head_file, head);
            std::getline(onto_file, onto);
        }

        auto filepath = m_repo_path + '/' + TODO_FILE_PATH;

        auto res = parse_file(filepath);
        if (!res.err.empty()) {
            QMessageBox::critical(this, "Rebase Error", res.err.c_str());
            return;
        }

        display_commit_actions(res.actions, head, onto);
    }

    void
    display_commit_actions(const std::vector<CommitAction>& actions, const std::string& head, const std::string& onto) {
        clear_layout(m_layout);

        std::vector<git_commit_t> commits;
        if (!get_all_commits_in_range(commits, head.c_str(), onto.c_str(), m_repo)) {
            QMessageBox::critical(this, "Rebase Error", "Cannot find commits");
            return;
        }

        auto* list_actions     = new QListWidget();
        auto* list_new_commits = new QListWidget();
        auto* list_old_commits = new QListWidget();

        // 1. prepare actions
        // 2. link old commits with coresponding actions
        std::unordered_map<std::string, ListItem*> all_actions;

        QListWidgetItem* last_item;
        {
            auto* start_commit = new QListWidgetItem(list_new_commits);
            auto commit_text   = format_commit(commits.front().commit);

            start_commit->setText(commit_text.c_str());
            list_new_commits->addItem(start_commit);

            auto id         = format_oid(commits.front().commit);
            std::string key = id.data();

            last_item = start_commit;
        }

        for (auto&& action : actions) {
            auto* item = new ListItem(list_actions);

            QString item_text = cmd_to_str(action.type);

            std::string key  = action.hash.substr(0, 7);
            all_actions[key] = item;

            switch (action.type) {
            case CmdType::INVALID:
            case CmdType::NONE:
            case CmdType::EXEC:
            case CmdType::BREAK:
                break;

            case CmdType::DROP:
                item->setItemColor(QColor(255, 62, 65));
                item_text += " ";
                item_text += action.hash.c_str();
                break;

            case CmdType::FIXUP:
            case CmdType::SQUASH:
                item->setItemColor(QColor(115, 137, 174));
                item->addConnection(last_item);
                item_text += " ";
                item_text += action.hash.c_str();
                break;

            case CmdType::RESET:
            case CmdType::LABEL:
            case CmdType::MERGE:
            case CmdType::UPDATE_REF:
                TODO("Implement");
                break;

            case CmdType::PICK:
            case CmdType::REWORD:
            case CmdType::EDIT:
                item->setItemColor(QColor(49, 216, 67));

                if (auto* item_commit = create_commit_item(action.hash.c_str())) {
                    list_new_commits->addItem(item_commit);
                    last_item = item_commit;

                    item->addConnection(item_commit);
                }

                item_text += " ";
                item_text += action.hash.c_str();
                break;
            }

            item->setText(item_text);
            list_actions->addItem(item);
        }

        for (auto&& commit : commits) {
            auto* item         = new QListWidgetItem(list_old_commits);
            auto commit_detail = format_commit(commit.commit);

            item->setText(commit_detail.c_str());

            auto id         = format_oid(commit.commit);
            std::string key = id.data();

            if (!all_actions.contains(key)) {
                continue;
            }

            auto* action = all_actions[key];
            action->addConnection(item);
        }

        m_layout->addWidget(list_actions);
        m_layout->addWidget(list_old_commits);
        m_layout->addWidget(list_new_commits);

        list_old_commits->setSelectionMode(QAbstractItemView::NoSelection);
        list_new_commits->setSelectionMode(QAbstractItemView::NoSelection);

        connect(list_actions, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
            if (m_last_item != nullptr) {
                m_last_item->setColorToAll(Qt::white);
            }

            if (item != nullptr) {
                auto* list_item = dynamic_cast<ListItem*>(item);
                if (list_item == nullptr) {
                    return;
                }

                list_item->setColorToAll(list_item->getItemColor());
                m_last_item = list_item;
            }
        });
    }

    QListWidgetItem* create_commit_item(const char* hash) {

        auto* item_commit = new QListWidgetItem();

        git_commit_t commit;
        if (!get_commit_from_hash(commit, hash, m_repo)) {
            QMessageBox::critical(this, "Rebase Error", QString("Commit no found: '") + hash + '\'');
            delete item_commit;
            return nullptr;
        }

        auto commit_detail = format_commit(commit.commit);
        item_commit->setText(commit_detail.c_str());

        return item_commit;
    }
};

int main(int argc, char* argv[]) {

    QApplication app(argc, argv);

    MainWindow main_window;
    main_window.show();

    return QApplication::exec();
}
