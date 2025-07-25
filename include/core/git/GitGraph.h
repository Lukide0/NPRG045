#pragma once

#include "types.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <git2/commit.h>
#include <git2/oid.h>
#include <git2/types.h>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace core::git {

template <typename Data> struct GitNode {
    commit_t commit;
    std::uint32_t depth;
    std::uint32_t index;
    std::set<std::uint32_t> children;
    Data data;
};

template <typename Data>
    requires(std::is_default_constructible_v<Data>)
class GitGraph {
public:
    using node_t = GitNode<Data>;

    static GitGraph empty() { return {}; }

    static std::optional<GitGraph> create(const char* start, const char* end, git_repository* repo) {

        commit_t start_commit;
        commit_t end_commit;

        if (!get_commit_from_hash(start_commit, start, repo) || !get_commit_from_hash(end_commit, end, repo)) {
            return std::nullopt;
        }

        GitGraph graph;
        // insert start(HEAD commit)
        auto start_index = graph.try_insert(std::move(start_commit), 0, {});

        // insert end(first commit)
        auto end_index = graph.try_insert(std::move(end_commit), 0, {});

        assert(start_index == 0);
        assert(end_index == 1);

        if (!try_create(graph, start_index, end_index, 0)) {
            return std::nullopt;
        }

        graph.sort_by_depth();

        return graph;
    }

    bool contains(const std::string& id) const { return m_commit_map.contains(id); }

    node_t& head() { return m_nodes[0]; }

    const node_t& head() const { return m_nodes[0]; }

    std::uint32_t max_depth() const { return m_nodes.rbegin()->depth; }

    std::uint32_t max_width() const { return m_max_width; }

    const node_t& first_node() const { return *m_nodes.rbegin(); }

    node_t& first_node() { return *m_nodes.rbegin(); }

    node_t& get(const std::string& id) { return m_nodes[get_index(id)]; }

    const node_t& get(const std::string& id) const { return m_nodes[get_index(id)]; }

    node_t& get(std::uint32_t index) { return m_nodes[index]; }

    const node_t& get(std::uint32_t index) const { return m_nodes[index]; }

    std::uint32_t get_index(const std::string& id) const {
        assert(contains(id));
        return m_commit_map.at(id);
    }

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

    static std::string get_commit_id(const git_commit* commit) {
        char buff[GIT_OID_MAX_HEXSIZE + 1] = { 0 };

        git_oid_tostr(buff, sizeof(buff) / sizeof(buff[0]), git_commit_id(commit));

        return { buff };
    }

private:
    std::unordered_map<std::string, std::uint32_t> m_commit_map;
    std::vector<node_t> m_nodes;
    std::uint32_t m_max_width = 0;

    GitGraph() = default;

    std::uint32_t try_insert(commit_t&& commit, std::uint32_t depth, const std::set<std::uint32_t>& children) {

        auto id = get_commit_id(commit);
        if (contains(id)) {
            return get_index(id);
        }

        std::uint32_t index = m_nodes.size();

        m_commit_map.emplace(id, index);

        m_nodes.push_back(
            node_t {
                .commit   = std::move(commit),
                .depth    = depth,
                .index    = index, // NOTE: used for sorting
                .children = children,
                .data     = {},
            }
        );

        return index;
    }

    static bool try_create(GitGraph& graph, std::uint32_t start_node, std::uint32_t end_node, std::uint32_t depth) {
        git_commit* start = graph.m_nodes[start_node].commit;
        git_commit* end   = graph.m_nodes[end_node].commit;

        if (check_commit(start, end)) {
            return true;
        }

        commit_parents_t parents;

        if (!get_all_parents(parents, start)) {
            return false;
        }

        // NOTE: The graph has new root node
        if (parents.count == 0) {
            node_t& node = graph.m_nodes[end_node];
            node.children.insert(start_node);
            node.depth = std::max(depth + 1, node.depth);
            return true;
        }
        // merge commit
        else if (parents.count != 1) {
            return false;
        }

        for (std::uint32_t i = 0; i < parents.count; ++i) {
            commit_t parent;

            // move ownership
            parent             = parents.parents[i];
            parents.parents[i] = nullptr;

            auto id       = get_commit_id(parent);
            bool contains = graph.contains(id);

            // prepare parent node
            std::uint32_t index = graph.try_insert(std::move(parent), depth + 1, {});
            node_t& node        = graph.get(index);

            node.children.insert(start_node);
            node.depth = std::max(depth + 1, node.depth);

            if (contains) {
                continue;
            }

            if (!try_create(graph, index, end_node, depth + 1)) {
                return false;
            }
        }

        return true;
    }

    void update_record(const node_t& node) {
        auto id          = get_commit_id(node.commit.get());
        m_commit_map[id] = node.index;
    }

    void sort_by_depth() {
        struct orig_info_t {
            std::uint32_t index;
            std::uint32_t depth;

            bool operator<(const orig_info_t& other) const { return depth < other.depth; }
        };

        std::vector<orig_info_t> vec;

        for (std::uint32_t i = 0; i < m_nodes.size(); ++i) {
            vec.push_back({ i, m_nodes[i].depth });
            m_nodes[i].index = i;
        }

        std::stable_sort(vec.begin(), vec.end());

        for (std::uint32_t i = 0; i < vec.size();) {

            std::uint32_t depth = vec[i].depth;

            std::uint32_t start = i;
            while (i < vec.size() && vec[i].depth == depth) {
                i += 1;
            }

            std::uint32_t count = i - start;
            m_max_width         = std::max(count, m_max_width);
        }

        std::vector<node_t> new_nodes;
        new_nodes.reserve(m_nodes.size());

        std::vector<uint32_t> child_map;
        child_map.resize(m_nodes.size());

        for (std::uint32_t i = 0; i < vec.size(); ++i) {
            auto pos    = vec[i];
            auto&& node = std::move(m_nodes[pos.index]);

            child_map[node.index] = i;

            node.index = i;
            update_record(node);

            new_nodes.push_back(std::move(node));
        }

        // Update children
        for (auto&& node : new_nodes) {
            std::set<uint32_t> new_chilren;
            for (auto&& child : node.children) {
                new_chilren.insert(child_map[child]);
            }
        }

        m_nodes = std::move(new_nodes);
    }
};

}
