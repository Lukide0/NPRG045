#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <git2/buffer.h>
#include <git2/merge.h>
#include <string>
#include <string_view>
#include <utility>

#include <git2/commit.h>
#include <git2/diff.h>
#include <git2/index.h>
#include <git2/object.h>
#include <git2/oid.h>
#include <git2/patch.h>
#include <git2/refs.h>
#include <git2/revparse.h>
#include <git2/signature.h>
#include <git2/tree.h>
#include <git2/types.h>

namespace core::git {

template <typename T> using destructor_t = void (*)(T*);

template <typename T, destructor_t<T> Destructor> class ptr_object_t {
public:
    ptr_object_t() = default;

    ptr_object_t(ptr_object_t&& other)
        : m_obj(other.m_obj) {
        other.m_obj = nullptr;
    }

    ptr_object_t& operator=(ptr_object_t&& other) {
        std::swap(m_obj, other.m_obj);
        return *this;
    }

    ptr_object_t& operator=(T* object) {
        if (m_obj != nullptr) {
            Destructor(m_obj);
        }

        m_obj = object;
        return *this;
    }

    ~ptr_object_t() { Destructor(m_obj); }

    operator T*() { return m_obj; }

    T* get() { return m_obj; }

    const T* get() const { return m_obj; }

    T* release() {
        T* tmp = m_obj;

        m_obj = nullptr;

        return tmp;
    }

    T** operator&() { return &m_obj; }

    const T** operator&() const { return &m_obj; }

private:
    T* m_obj = nullptr;
};

template <typename T, destructor_t<T> Destructor> class object_t {
public:
    object_t(T&& obj)
        : m_obj(std::move(obj)) { }

    object_t(object_t&& other) { std::swap(m_obj, other.m_obj); }

    object_t& operator=(object_t&& other) {
        std::swap(m_obj, other.m_obj);
        return *this;
    }

    ~object_t() { Destructor(&m_obj); }

    T* operator&() { return &m_obj; }

    T& get() { return m_obj; }

    const T& get() const { return m_obj; }

private:
    T m_obj;
};

template <typename T, destructor_t<T> Destructor> class weak_object_t {
public:
    weak_object_t() = default;

    weak_object_t(weak_object_t&& other) { std::swap(m_obj, other.m_obj); }

    weak_object_t& operator=(weak_object_t&& other) {
        std::swap(m_obj, other.m_obj);
        return *this;
    }

    T* operator&() { return &m_obj; }

    T& get() { return m_obj; }

    const T& get() const { return m_obj; }

    object_t<T, Destructor> to_object() { return object_t<T, Destructor>(std::move(m_obj)); }

private:
    T m_obj;
};

using commit_t            = ptr_object_t<git_commit, git_commit_free>;
using index_t             = ptr_object_t<git_index, git_index_free>;
using tree_t              = ptr_object_t<git_tree, git_tree_free>;
using signature_t         = ptr_object_t<git_signature, git_signature_free>;
using reference_t         = ptr_object_t<git_reference, git_reference_free>;
using diff_t              = ptr_object_t<git_diff, git_diff_free>;
using patch_t             = ptr_object_t<git_patch, git_patch_free>;
using conflict_iterator_t = ptr_object_t<git_index_conflict_iterator, git_index_conflict_iterator_free>;

using buffer_t            = object_t<git_buf, git_buf_dispose>;
using merge_file_result_t = weak_object_t<git_merge_file_result, git_merge_file_result_free>;

struct commit_parents_t {
    git_commit** parents = nullptr;
    unsigned int count   = 0;

    [[nodiscard]] const git_commit** get() const { return const_cast<const git_commit**>(parents); }

    ~commit_parents_t() {
        if (count > 0) {
            for (std::size_t i = 0; i < count; ++i) {
                git_commit_free(parents[i]);
            }

            delete[] parents;
        }
    }
};

inline bool get_commit_from_hash(commit_t& out_commit, const char* hash, git_repository* repo) {

    git_object* obj = nullptr;

    if (git_revparse_single(&obj, repo, hash) != 0 || git_commit_lookup(&out_commit, repo, git_object_id(obj)) != 0) {
        git_object_free(obj);
        return false;
    }

    return true;
}

inline bool get_oid_from_hash(git_oid& out_oid, const char* hash, git_repository* repo) {
    git_object* obj = nullptr;

    if (git_revparse_single(&obj, repo, hash) != 0) {
        return false;
    }

    out_oid = *git_object_id(obj);
    git_object_free(obj);
    return true;
}

inline bool get_last_commit(commit_t& out_commit, git_repository* repo) {
    git_oid id;
    return git_reference_name_to_id(&id, repo, "HEAD") == 0 && git_commit_lookup(&out_commit, repo, &id) == 0;
}

inline bool get_all_parents(commit_parents_t& parents, git_commit* commit) {
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

template <std::uint8_t HashSize = 7> std::array<char, HashSize + 1> format_oid(const git_oid* id) {
    // NOTE: +1 is for '\0'
    std::array<char, HashSize + 1> buff;
    git_oid_tostr(buff.data(), buff.size(), id);

    return buff;
}

template <std::uint8_t HashSize = 7> std::array<char, HashSize + 1> format_oid(const git_commit* commit) {
    return format_oid<HashSize>(git_commit_id(commit));
}

template <std::uint8_t HashSize = 7> std::string format_oid_to_str(const git_oid* oid) {
    auto arr = format_oid<HashSize>(oid);
    return std::string(arr.begin());
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
}
