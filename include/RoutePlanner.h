// ===== MEMBER 3 (Arya Kumari) : picking-route planning on top of Dijkstra =====
// Dijkstra alone only answers "shortest path between TWO nodes". Picking an order with several
// shelves also needs a VISIT ORDER (a small travelling-salesman-style problem, NP-hard in general).
// We therefore build a shortest-path distance matrix between the stops with Dijkstra, then choose the
// order with one of three policies:
//   NAIVE         : visit shelves in the order the order lines were typed (baseline)
//   NEAREST       : nearest-neighbour heuristic (greedy)
//   NEAREST_2OPT  : nearest-neighbour, then 2-opt segment reversals until no improvement
// None of these is guaranteed optimal; 2-opt is never worse than nearest-neighbour by construction.
#pragma once
#include "Graph.h"
#include <algorithm>
#include <numeric>
#include <vector>

enum class PickPolicy { NAIVE, NEAREST, NEAREST_2OPT };

inline const char* toString(PickPolicy p) {
    switch (p) {
        case PickPolicy::NAIVE:   return "naive";
        case PickPolicy::NEAREST: return "nearest-neighbour";
        default:                  return "nn+2opt";
    }
}

struct RoutePlan {
    bool feasible = false;               // false if a stop is invalid/unreachable
    std::vector<int> stops;              // distinct shelf nodes in visiting order
    double pickDistance = Graph::kInf;   // depot -> stops -> packing
    std::vector<int> path;               // full node-by-node walk including intermediate nodes
};

class RoutePlanner {
public:
    // The graph must not change after the planner is constructed (packing distances are cached).
    RoutePlanner(const Graph& g, int depot, int packing)
        : g_(g), depot_(depot), packing_(packing), fromPacking_(g.dijkstra(packing)) {}

    RoutePlan planPicking(const std::vector<int>& itemNodes, PickPolicy policy) const {
        RoutePlan plan;
        if (!g_.valid(depot_) || !g_.valid(packing_)) return plan;

        std::vector<int> stops;                                  // de-duplicate, keep first-seen order
        for (int n : itemNodes) {
            if (!g_.valid(n)) return plan;
            if (std::find(stops.begin(), stops.end(), n) == stops.end()) stops.push_back(n);
        }
        const int k = static_cast<int>(stops.size());
        const int m = k + 2;                                      // depot, stops..., packing
        std::vector<int> nodes;
        nodes.push_back(depot_);
        nodes.insert(nodes.end(), stops.begin(), stops.end());
        nodes.push_back(packing_);

        std::vector<Graph::ShortestPaths> sp;                     // one Dijkstra per source
        for (int i = 0; i < m - 1; ++i) sp.push_back(g_.dijkstra(nodes[i]));
        auto D = [&](int a, int b) { return sp[a].dist[nodes[b]]; };
        auto total = [&](const std::vector<int>& ord) {
            double t = 0.0; int prev = 0;
            for (int i : ord) { t += D(prev, i); prev = i; }
            return t + D(prev, m - 1);
        };

        std::vector<int> order(k);
        std::iota(order.begin(), order.end(), 1);                 // indices into `nodes`

        if (policy != PickPolicy::NAIVE) {
            std::vector<int> remaining = order, result;
            int cur = 0;
            while (!remaining.empty()) {
                std::size_t best = 0;
                for (std::size_t j = 1; j < remaining.size(); ++j)
                    if (D(cur, remaining[j]) < D(cur, remaining[best])) best = j;   // ties: earlier wins
                cur = remaining[best];
                result.push_back(cur);
                remaining.erase(remaining.begin() + best);
            }
            order = result;
        }
        if (policy == PickPolicy::NEAREST_2OPT && total(order) < Graph::kInf) {
            bool improved = true;
            while (improved) {
                improved = false;
                for (int i = 0; i + 1 < k; ++i)
                    for (int j = i + 1; j < k; ++j) {
                        std::vector<int> cand = order;
                        std::reverse(cand.begin() + i, cand.begin() + j + 1);
                        if (total(cand) < total(order) - 1e-9) { order = cand; improved = true; }
                    }
            }
        }

        plan.pickDistance = total(order);
        if (plan.pickDistance == Graph::kInf) return plan;       // some stop unreachable

        std::vector<int> seq;
        seq.push_back(0);
        seq.insert(seq.end(), order.begin(), order.end());
        seq.push_back(m - 1);
        plan.path.push_back(depot_);
        for (std::size_t t = 0; t + 1 < seq.size(); ++t) {
            std::vector<int> seg = Graph::buildPath(sp[seq[t]], nodes[seq[t + 1]]);
            for (std::size_t q = 1; q < seg.size(); ++q) plan.path.push_back(seg[q]);
        }
        for (int i : order) plan.stops.push_back(nodes[i]);
        plan.feasible = true;
        return plan;
    }

    double deliveryDistance(int dest) const {
        return g_.valid(dest) ? fromPacking_.dist[dest] : Graph::kInf;
    }
    std::vector<int> deliveryPath(int dest) const { return Graph::buildPath(fromPacking_, dest); }

private:
    const Graph& g_;
    int depot_, packing_;
    Graph::ShortestPaths fromPacking_;
};
