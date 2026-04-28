#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <git2/blob.h>
#include <git2/buffer.h>
#include <git2/commit.h>
#include <git2/diff.h>
#include <git2/index.h>
#include <git2/merge.h>
#include <git2/object.h>
#include <git2/oid.h>
#include <git2/patch.h>
#include <git2/refs.h>
#include <git2/repository.h>
#include <git2/revparse.h>
#include <git2/signature.h>
#include <git2/status.h>
#include <git2/tree.h>
#include <git2/types.h>

namespace core::git {

/**
 * @brief Size of a Git OID string representation.
 */
constexpr std::size_t OID_SIZE = 40;

template <typename T> using destructor_t = void (*)(T*);

/**
 * @brief Smart pointer wrapper for Git objects with custom destructor.
 *
 * @tparam T Git object type.
 * @tparam Destructor Free function for the Git object.
 */
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

    ~ptr_object_t() { destroy(); }

    operator T*() { return m_obj; }

    T* get() { return m_obj; }

    const T* get() const { return m_obj; }

    T* release() {
        T* tmp = m_obj;

        m_obj = nullptr;

        return tmp;
    }

    void destroy() {
        if (m_obj != nullptr) {
            Destructor(m_obj);
        }

        m_obj = nullptr;
    }

    T** operator&() { return &m_obj; }

    const T** operator&() const { return &m_obj; }

private:
    T* m_obj = nullptr;
};

/**
 * @brief Value-owning wrapper for Git-related objects.
 *
 * @tparam T Object type.
 * @tparam Destructor Free function for the object.
 */
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

using commit_t            = ptr_object_t<git_commit, git_commit_free>;
using index_t             = ptr_object_t<git_index, git_index_free>;
using tree_t              = ptr_object_t<git_tree, git_tree_free>;
using signature_t         = ptr_object_t<git_signature, git_signature_free>;
using reference_t         = ptr_object_t<git_reference, git_reference_free>;
using diff_t              = ptr_object_t<git_diff, git_diff_free>;
using patch_t             = ptr_object_t<git_patch, git_patch_free>;
using status_list_t       = ptr_object_t<git_status_list, git_status_list_free>;
using conflict_iterator_t = ptr_object_t<git_index_conflict_iterator, git_index_conflict_iterator_free>;
using index_iterator_t    = ptr_object_t<git_index_iterator, git_index_iterator_free>;
using blob_t              = ptr_object_t<git_blob, git_blob_free>;
using repository_t        = ptr_object_t<git_repository, git_repository_free>;

using buffer_t = object_t<git_buf, git_buf_dispose>;

/**
 * @brief Converts a list of strings to a Git string array.
 */
class str_array {
public:
    str_array(std::span<const std::string> strings)
        : m_storage(strings.begin(), strings.end()) {
        m_size    = strings.size();
        m_strings = std::make_unique<char*[]>(m_size);

        for (std::size_t i = 0; i < m_size; ++i) {
            m_strings[i] = m_storage[i].data();
        }
    }

    git_strarray get_array() {
        git_strarray arr;
        arr.count   = m_size;
        arr.strings = m_strings.get();

        return arr;
    }

    void fill(git_strarray& arr) {
        arr.count   = m_size;
        arr.strings = m_strings.get();
    }

private:
    std::vector<std::string> m_storage;
    std::unique_ptr<char*[]> m_strings;
    std::size_t m_size;
};

/**
 * @brief Looks up a commit from a hash.
 *
 * @param out_commit Resulting commit object.
 * @param hash Revision string.
 * @param repo Git repository.
 *
 * @return True if lookup succeeded.
 */
inline bool get_commit_from_hash(commit_t& out_commit, const char* hash, git_repository* repo) {

    git_object* obj = nullptr;

    if (git_revparse_single(&obj, repo, hash) != 0 || git_commit_lookup(&out_commit, repo, git_object_id(obj)) != 0) {
        git_object_free(obj);
        return false;
    }

    return true;
}

/**
 * @brief Resolves a Git object ID from a hash.
 *
 * @param out_oid Output object ID.
 * @param hash Revision string.
 * @param repo Git repository.
 *
 * @return True if successful.
 */
inline bool get_oid_from_hash(git_oid& out_oid, const char* hash, git_repository* repo) {
    git_object* obj = nullptr;

    if (git_revparse_single(&obj, repo, hash) != 0) {
        return false;
    }

    out_oid = *git_object_id(obj);
    git_object_free(obj);
    return true;
}

/**
 * @brief Formats a Git OID into a short string.
 *
 * @tparam HashSize Length of hash prefix.
 * @param id Object ID.
 *
 * @return Formatted OID string.
 */
template <std::uint8_t HashSize = 7> std::array<char, HashSize + 1> format_oid(const git_oid* id) {
    // NOTE: +1 is for '\0'
    std::array<char, HashSize + 1> buff;
    git_oid_tostr(buff.data(), buff.size(), id);

    return buff;
}

/**
 * @brief Formats a commit's OID into a short string.
 *
 * @tparam HashSize Length of hash prefix.
 * @param commit Commit object.
 *
 * @return Formatted OID string.
 */
template <std::uint8_t HashSize = 7> std::array<char, HashSize + 1> format_oid(const git_commit* commit) {
    return format_oid<HashSize>(git_commit_id(commit));
}

/**
 * @brief Converts a Git OID to string.
 *
 * @tparam HashSize Length of hash prefix.
 * @param oid Object ID.
 *
 * @return String representation of OID.
 */
template <std::uint8_t HashSize = 7> std::string format_oid_to_str(const git_oid* oid) {
    auto arr = format_oid<HashSize>(oid);
    return std::string(arr.begin());
}

}
