#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <git2/commit.h>
#include <git2/diff.h>
#include <git2/index.h>
#include <git2/object.h>
#include <git2/oid.h>
#include <git2/refs.h>
#include <git2/revparse.h>
#include <git2/signature.h>
#include <git2/tree.h>
#include <git2/types.h>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct git_commit_t {
    git_commit* commit = nullptr;

    git_commit_t() = default;

    git_commit_t(git_commit_t&& other)
        : commit(other.commit) {
        other.commit = nullptr;
    }

    ~git_commit_t() { git_commit_free(commit); }
};

struct git_index_t {
    git_index* index = nullptr;

    git_index_t() = default;

    git_index_t(git_index_t&& other)
        : index(other.index) {
        other.index = nullptr;
    }

    git_index_t& operator=(git_index_t&& other) {
        std::swap(index, other.index);
        return *this;
    }

    git_index_t& operator=(git_index* other) {
        if (index != nullptr) {
            git_index_free(index);
        }

        index = other;
        return *this;
    }

    ~git_index_t() { git_index_free(index); }
};

struct git_tree_t {
    git_tree* tree = nullptr;

    git_tree_t() = default;

    git_tree_t(git_tree_t&& other)
        : tree(other.tree) {
        other.tree = nullptr;
    }

    git_tree_t& operator=(git_tree_t&& other) {
        std::swap(tree, other.tree);
        return *this;
    }

    git_tree_t& operator=(git_tree* other) {
        if (tree != nullptr) {
            git_tree_free(tree);
        }

        tree = other;
        return *this;
    }

    ~git_tree_t() { git_tree_free(tree); }
};

struct git_signature_t {
    git_signature* sig = nullptr;

    ~git_signature_t() { git_signature_free(sig); }
};

struct git_reference_obj {
    git_reference* ref = nullptr;

    ~git_reference_obj() { git_reference_free(ref); }
};

struct git_diff_t {
    git_diff* diff = nullptr;

    ~git_diff_t() { git_diff_free(diff); }
};

struct git_commit_parents_t {
    git_commit** parents = nullptr;
    unsigned int count   = 0;

    [[nodiscard]] const git_commit** get() const { return const_cast<const git_commit**>(parents); }

    ~git_commit_parents_t() {
        if (count > 0) {
            for (std::size_t i = 0; i < count; ++i) {
                git_commit_free(parents[i]);
            }

            delete[] parents;
        }
    }
};

inline bool get_commit_from_hash(git_commit_t& out_commit, const char* hash, git_repository* repo) {

    git_object* obj = nullptr;

    if (git_revparse_single(&obj, repo, hash) != 0
        || git_commit_lookup(&out_commit.commit, repo, git_object_id(obj)) != 0) {
        git_object_free(obj);
        return false;
    }

    return true;
}

inline bool get_last_commit(git_commit_t& out_commit, git_repository* repo) {
    git_oid id;
    return git_reference_name_to_id(&id, repo, "HEAD") == 0 && git_commit_lookup(&out_commit.commit, repo, &id) == 0;
}

inline bool get_all_parents(git_commit_parents_t& parents, git_commit* commit) {
    const auto parents_count = git_commit_parentcount(commit);

    auto** parent_commits = new git_commit*[parents_count];

    for (std::size_t i = 0; i < parents_count; ++i) {
        if (git_commit_parent(&parent_commits[i], commit, i) != 0) {
            delete[] parent_commits;
            return false;
        }
    }

    parents.parents = parent_commits;
    parents.count   = parents_count;

    return true;
}

inline bool check_commit(git_commit* a, git_commit* b) {
    const auto* id_a = git_commit_id(a);
    const auto* id_b = git_commit_id(b);

    return git_oid_equal(id_a, id_b) != 0;
}

template <std::uint8_t HashSize = 7> std::array<char, HashSize + 1> format_oid(const git_commit* commit) {
    // NOTE: +1 is for '\0'
    std::array<char, HashSize + 1> buff;
    git_oid_tostr(buff.data(), buff.size(), git_commit_id(commit));

    return buff;
}

template <std::uint8_t HashSize = 7> std::array<char, HashSize + 1> format_oid(const git_oid* id) {
    // NOTE: +1 is for '\0'
    std::array<char, HashSize + 1> buff;
    git_oid_tostr(buff.data(), buff.size(), id);

    return buff.data();
}

template <std::uint8_t HashSize = 7> std::string format_commit(git_commit* commit) {
    std::string name;

    const auto* id = git_commit_id(commit);
    std::array<char, HashSize + 1> buff;

    name += git_oid_tostr(buff.data(), buff.size(), id);
    name += ": ";

    const char* msg = git_commit_summary(commit);
    name += msg;

    return name;
}
