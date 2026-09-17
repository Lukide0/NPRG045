#pragma once

#include "action/Action.h"
#include "conflict/ConflictManager.h"
#include "git/types.h"
#include <git2/oid.h>
#include <git2/types.h>
#include <string>
#include <vector>

namespace state {

class AppState {
public:
    class Conflict {
    public:
        void set_conflict(action::Action* act, git::index_t&& index) {
            m_action = act;
            m_index  = std::move(index);
        }

        void finish_resolving() { m_resolving = false; }

        void start_resolving() { m_resolving = true; }

        void clear_resolving() { m_resolving = false; }

        void clear_conflict() {
            m_action = nullptr;
            clear_items();
        }

        void clear_items() {
            m_conflict_paths.clear();
            m_conflict_entries.clear();
            m_conflict_files.clear();
        }

        void add(const std::string& path, const conflict::ConflictEntry& entry) {
            m_conflict_paths.push_back(path);
            m_conflict_entries.push_back(entry);
        }

        void add_file(const git_oid& file) { m_conflict_files.push_back(file); }

        git_index* index() { return m_index; }

        [[nodiscard]] const action::Action* action() const { return m_action; }

        action::Action* action() { return m_action; }

        [[nodiscard]] action::Action* parent_action() {
            if (m_action != nullptr) {
                return m_action->get_prev();
            } else {
                return nullptr;
            }
        }

        [[nodiscard]] const auto& paths() const { return m_conflict_paths; }

        [[nodiscard]] const auto& entries() const { return m_conflict_entries; }

        [[nodiscard]] const auto& files() const { return m_conflict_files; }

        [[nodiscard]] bool is_resolving() const { return m_resolving; }

    private:
        bool m_resolving         = false;
        action::Action* m_action = nullptr;
        git::index_t m_index;

        std::vector<std::string> m_conflict_paths;
        std::vector<conflict::ConflictEntry> m_conflict_entries;
        std::vector<git_oid> m_conflict_files;
    };

    struct Repo {
        git::repository_t repo;

        std::string path;
        std::string rebase_head;
        std::string rebase_onto;
    };

    AppState() = default;

    void set_repo(git::repository_t&& repo, const std::string& path) {
        m_repo.repo = std::move(repo);
        m_repo.path = path;
    }

    void set_rebase(const std::string& head, const std::string& onto) {
        m_repo.rebase_head = head;
        m_repo.rebase_onto = onto;
    }

    git_repository* repo() { return m_repo.repo; }

    [[nodiscard]] const std::string& rebase_head() const { return m_repo.rebase_head; }

    [[nodiscard]] const std::string& rebase_onto() const { return m_repo.rebase_onto; }

    [[nodiscard]] const std::string& repo_path() const { return m_repo.path; }

    [[nodiscard]] Conflict& conflict() { return m_conflict; }

    [[nodiscard]] const Conflict& conflict() const { return m_conflict; }

private:
    Repo m_repo;
    Conflict m_conflict;
};

}
