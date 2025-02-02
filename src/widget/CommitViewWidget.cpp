#include "widget/CommitViewWidget.h"
#include "git/GitGraph.h"
#include "git/types.h"
#include "widget/graph/Node.h"
#include <chrono>
#include <ctime>
#include <format>
#include <git2/commit.h>
#include <git2/diff.h>
#include <git2/tree.h>
#include <git2/types.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

void CommitViewWidget::create_rows() {

    git_commit* commit = m_node->getCommit();

    std::string hash = GitGraph<Node*>::get_commit_id(commit);

    const char* summary        = git_commit_summary(commit);
    const char* desc           = git_commit_body(commit);
    const git_signature* autor = git_commit_author(commit);

    std::stringstream ss;
    {
        std::chrono::seconds seconds { autor->when.time };
        std::time_t time = seconds.count();
        std::tm* t       = std::gmtime(&time);

        ss << std::put_time(t, "%x %X");
    }
    auto time_str = ss.str();

    create_row("Hash:", hash.c_str(), 0);
    create_row("Author:", autor->name, 1);
    create_row("Date:", time_str.c_str(), 2);
    create_row("Summary:", summary, 3);
    create_row("Description:", desc, 4);
}

void CommitViewWidget::prepare_diff() {

    Node* parentNode = m_node->getParentNode();
    if (parentNode == nullptr) {
        std::cout << "NO PARENT FOR NODE:" << m_node << "\n";
        return;
    }

    git_commit* curr   = m_node->getCommit();
    git_commit* parent = parentNode->getCommit();

    git_tree_t curr_tree;
    git_tree_t parent_tree;

    if (git_commit_tree(&curr_tree.tree, curr) != 0 || git_commit_tree(&parent_tree.tree, parent) != 0) {
        std::cout << "FAILED TO FIND TREES\n";
        return;
    }

    git_diff_t diff;
    git_repository* repo = git_tree_owner(curr_tree.tree);
    if (git_diff_tree_to_tree(&diff.diff, repo, parent_tree.tree, curr_tree.tree, nullptr) != 0) {
        std::cout << "FAILED TO FIND DIFF\n";
        return;
    }

    std::cout << "----------------- DIFF START -----------------\n";
    git_diff_foreach(
        diff.diff,
        [](const git_diff_delta* delta, float progress, void*) -> int {
            std::cout << std::format("Old file: {}\nNew file: {}\n", delta->old_file.path, delta->new_file.path);
            std::cout << std::format("Progress: {}\n", progress);

            std::cout << "Status: ";
            switch (delta->status) {
            case GIT_DELTA_UNMODIFIED:
                std::cout << "UNMODIFIED\n";
                break;
            case GIT_DELTA_ADDED:
                std::cout << "ADDED\n";
                break;
            case GIT_DELTA_DELETED:
                std::cout << "DELETED\n";
                break;
            case GIT_DELTA_MODIFIED:
                std::cout << "MODIFIED\n";
                break;
            case GIT_DELTA_RENAMED:
                std::cout << "RENAMED\n";
                break;
            case GIT_DELTA_COPIED:
                std::cout << "COPIED\n";
                break;
            case GIT_DELTA_IGNORED:
                std::cout << "IGNORED\n";
                break;
            case GIT_DELTA_UNTRACKED:
                std::cout << "UNTRACKED\n";
                break;
            case GIT_DELTA_TYPECHANGE:
                std::cout << "TYPECHANGE\n";
                break;
            case GIT_DELTA_UNREADABLE:
                std::cout << "UNREADABLE\n";
                break;
            case GIT_DELTA_CONFLICTED:
                std::cout << "CONFLICTED\n";
                break;
            }
            std::cout << "-----------------\n";

            return 0;
        },
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
}
