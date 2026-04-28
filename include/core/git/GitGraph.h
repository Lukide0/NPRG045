#pragma once

#include "types.h"
#include <cassert>
#include <cstdint>
#include <functional>
#include <git2/commit.h>
#include <git2/oid.h>
#include <git2/revwalk.h>
#include <git2/types.h>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace core::git {

/**
 * @brief Node inside a Git graph.
 *
 * @tparam Data User-defined payload stored per node.
 */
template <typename Data> struct GitNode {
    commit_t commit;
    std::uint32_t depth;
    Data data;
};

/**
 * @brief Represents a Git commit graph between two commits.
 *
 * @tparam Data User-defined node data.
 */
template <typename Data>
    requires(std::is_default_constructible_v<Data>)
class GitGraph {
public:
    using node_t = GitNode<Data>;

    /**
     * @brief Creates an empty graph.
     */
    static GitGraph empty() { return {}; }

    /**
     * @brief Builds a graph from two commit hashes.
     *
     * @param start Start commit hash.
     * @param end End commit hash.
     * @param repo Git repository.
     *
     * @return GitGraph if successful, std::nullopt otherwise.
     */
    static std::optional<GitGraph> create(const char* start, const char* end, git_repository* repo) {

        commit_t start_commit;
        commit_t end_commit;

        if (!get_commit_from_hash(start_commit, start, repo) || !get_commit_from_hash(end_commit, end, repo)) {
            return std::nullopt;
        }

        GitGraph graph;

        git_revwalk* walker = nullptr;
        if (git_revwalk_new(&walker, repo) != 0) {
            return std::nullopt;
        }

        if (git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME) != 0) {
            git_revwalk_free(walker);
            return std::nullopt;
        }

        // start commit
        if (git_revwalk_push(walker, git_commit_id(start_commit)) != 0) {
            git_revwalk_free(walker);
            return std::nullopt;
        }

        // end commit
        if (git_revwalk_hide(walker, git_commit_id(end_commit)) != 0) {
            git_revwalk_free(walker);
            return std::nullopt;
        }

        git_oid oid;
        std::uint32_t idx = 0;

        // [start_commit..end_commit)
        while (git_revwalk_next(&oid, walker) == 0) {
            commit_t commit;

            if (git_commit_lookup(&commit, repo, &oid) != 0) {
                git_revwalk_free(walker);
                return std::nullopt;
            }

            // skip merge commits
            if (git_commit_parentcount(commit) > 1) {
                continue;
            }

            graph.try_insert(std::move(commit), idx);
            idx += 1;
        }

        // insert oldest commit
        graph.try_insert(std::move(end_commit), idx);

        git_revwalk_free(walker);
        return graph;
    }

    /**
     * @brief Checks whether a commit exists in the graph.
     *
     * @param id Commit ID.
     */
    bool contains(const std::string& id) const { return m_commit_map.contains(id); }

    /**
     * @brief Gets the first node in traversal order.
     */
    node_t& head() { return m_nodes[0]; }

    const node_t& head() const { return m_nodes[0]; }

    /**
     * @brief Gets maximum depth of the graph.
     */
    std::uint32_t max_depth() const { return m_nodes.rbegin()->depth; }

    /**
     * @brief Gets last node in graph.
     */
    const node_t& first_node() const { return *m_nodes.rbegin(); }

    node_t& first_node() { return *m_nodes.rbegin(); }

    /**
     * @brief Gets node by commit ID.
     *
     * @param id Commit ID.
     */
    node_t& get(const std::string& id) { return m_nodes[get_index(id)]; }

    const node_t& get(const std::string& id) const { return m_nodes[get_index(id)]; }

    /**
     * @brief Gets node by index.
     *
     * @param index Node index.
     */
    node_t& get(std::uint32_t index) { return m_nodes[index]; }

    const node_t& get(std::uint32_t index) const { return m_nodes[index]; }

    /**
     * @brief Gets index of a commit ID.
     *
     * @param id Commit ID.
     */
    std::uint32_t get_index(const std::string& id) const {
        assert(contains(id));
        return m_commit_map.at(id);
    }

    /**
     * @brief Iterates over graph grouped by depth (forward).
     *
     * @param fn Callback receiving depth and node span.
     */
    void iterate(std::function<void(std::uint32_t, std::span<node_t>)> fn) {
        for (std::uint32_t i = 0; i < m_nodes.size();) {

            std::uint32_t depth = m_nodes[i].depth;

            std::uint32_t start = i;
            while (i < m_nodes.size() && m_nodes[i].depth == depth) {
                i += 1;
            }

            fn(depth, std::span<node_t>(m_nodes.begin() + start, m_nodes.begin() + i));
        }
    }

    /**
     * @brief Iterates over graph grouped by depth (reverse).
     *
     * @param fn Callback receiving depth and node span.
     */
    void reverse_iterate(std::function<void(std::uint32_t, std::span<node_t>)> fn) {
        for (std::int32_t i = m_nodes.size() - 1; i >= 0;) {

            std::uint32_t depth = m_nodes[i].depth;

            std::uint32_t end = i;
            while (i >= 0 && m_nodes[i].depth == depth) {
                i -= 1;
            }

            fn(depth, std::span<node_t>(m_nodes.begin() + i + 1, m_nodes.begin() + end + 1));
        }
    }

private:
    std::unordered_map<std::string, std::uint32_t> m_commit_map;
    std::vector<node_t> m_nodes;

    GitGraph() = default;

    static std::string commit_id(const git_commit* commit) { return format_oid_to_str(git_commit_id(commit)); }

    std::uint32_t try_insert(commit_t&& commit, std::uint32_t depth) {

        auto id = commit_id(commit);
        if (contains(id)) {
            return get_index(id);
        }

        std::uint32_t index = m_nodes.size();
        m_commit_map.emplace(id, index);

        m_nodes.push_back(
            node_t {
                .commit = std::move(commit),
                .depth  = depth,
                .data   = {},
            }
        );

        return index;
    }
};

}
