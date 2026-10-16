// ===== MEMBER 3 (Arya Kumari) : weighted graph (adjacency list) + Dijkstra =====
#pragma once
#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

class Graph {
public:
    static constexpr double kInf = std::numeric_limits<double>::infinity();
    struct Edge { int to; double w; };
    struct ShortestPaths { std::vector<double> dist; std::vector<int> prev; };

    int addNode(const std::string& name) {
        names_.push_back(name);
        adj_.emplace_back();
        return static_cast<int>(names_.size()) - 1;
    }
    // Rejects bad node ids and negative weights (Dijkstra is invalid with negative edges).
    bool addEdge(int u, int v, double w, bool bidirectional = true) {
        if (!valid(u) || !valid(v) || w < 0.0) return false;
        adj_[u].push_back({v, w});
        if (bidirectional && u != v) adj_[v].push_back({u, w});
        return true;
    }
    int findNode(const std::string& name) const {      // -1 if absent
        for (int i = 0; i < nodeCount(); ++i) if (names_[i] == name) return i;
        return -1;
    }
    // Smallest direct edge weight u->v, kInf if there is no direct edge.
    double edgeWeight(int u, int v) const {
        double best = kInf;
        if (!valid(u) || !valid(v)) return best;
        for (const Edge& e : adj_[u]) if (e.to == v && e.w < best) best = e.w;
        return best;
    }
    int nodeCount() const { return static_cast<int>(adj_.size()); }
    const std::string& name(int id) const { return names_.at(id); }
    bool valid(int id) const { return id >= 0 && id < nodeCount(); }

    // O((V+E) log V) with a binary-heap priority queue.
    ShortestPaths dijkstra(int src) const {
        ShortestPaths sp{std::vector<double>(nodeCount(), kInf), std::vector<int>(nodeCount(), -1)};
        if (!valid(src)) return sp;
        using QItem = std::pair<double, int>;
        std::priority_queue<QItem, std::vector<QItem>, std::greater<QItem>> pq;
        sp.dist[src] = 0.0;
        pq.push({0.0, src});
        while (!pq.empty()) {
            auto [d, u] = pq.top(); pq.pop();
            if (d > sp.dist[u]) continue;                 // stale entry
            for (const Edge& e : adj_[u]) {
                double nd = d + e.w;
                if (nd < sp.dist[e.to]) { sp.dist[e.to] = nd; sp.prev[e.to] = u; pq.push({nd, e.to}); }
            }
        }
        return sp;
    }
    double distance(int src, int dst) const {
        if (!valid(src) || !valid(dst)) return kInf;
        return dijkstra(src).dist[dst];
    }
    // Rebuild the node sequence src..dst from a Dijkstra result; empty if unreachable.
    static std::vector<int> buildPath(const ShortestPaths& sp, int dst) {
        if (dst < 0 || dst >= static_cast<int>(sp.dist.size()) || sp.dist[dst] == kInf) return {};
        std::vector<int> p;
        for (int at = dst; at != -1; at = sp.prev[at]) p.push_back(at);
        std::reverse(p.begin(), p.end());
        return p;
    }
    // Node sequence src..dst; empty if unreachable or invalid.
    std::vector<int> path(int src, int dst) const {
        if (!valid(src) || !valid(dst)) return {};
        return buildPath(dijkstra(src), dst);
    }
private:
    std::vector<std::vector<Edge>> adj_;
    std::vector<std::string> names_;
};
