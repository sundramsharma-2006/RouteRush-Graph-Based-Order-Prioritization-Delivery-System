// ===== MEMBER 3 (Arya Kumari) =====
#include "RoutePlanner.h"
#include "WarehouseLayout.h"
#include "test_framework.h"
#include <random>

// Line graph  Depot -- 1 -- 2 -- 3 -- Packing  (all edges 10). Visiting 3 first then 1 is wasteful.
static Graph lineGraph(int& depot, int& packing) {
    Graph g;
    depot = g.addNode("Depot");
    int s1 = g.addNode("S1"), s2 = g.addNode("S2"), s3 = g.addNode("S3");
    packing = g.addNode("Packing");
    g.addEdge(depot, s1, 10); g.addEdge(s1, s2, 10); g.addEdge(s2, s3, 10); g.addEdge(s3, packing, 10);
    return g;
}

TEST(planner_nearest_beats_naive_on_line) {
    int depot, packing; Graph g = lineGraph(depot, packing);
    RoutePlanner rp(g, depot, packing);
    std::vector<int> items = {3, 1, 2};                    // typed in a bad order
    RoutePlan naive = rp.planPicking(items, PickPolicy::NAIVE);
    RoutePlan nn = rp.planPicking(items, PickPolicy::NEAREST);
    CHECK(naive.feasible && nn.feasible);
    CHECK((naive.stops == std::vector<int>{3, 1, 2}));
    CHECK((nn.stops == std::vector<int>{1, 2, 3}));
    CHECK(nn.pickDistance == 40.0);                        // optimal: straight through
    CHECK(naive.pickDistance == 30 + 20 + 10 + 20);        // D->S3 (30), S3->S1 (20), S1->S2 (10), S2->Packing (20)
    CHECK(nn.pickDistance < naive.pickDistance);
}

TEST(planner_dedupes_and_handles_empty) {
    int depot, packing; Graph g = lineGraph(depot, packing);
    RoutePlanner rp(g, depot, packing);
    RoutePlan p = rp.planPicking({2, 2, 2}, PickPolicy::NEAREST_2OPT);
    CHECK(p.feasible && p.stops.size() == 1 && p.pickDistance == 40.0);
    RoutePlan e = rp.planPicking({}, PickPolicy::NEAREST);
    CHECK(e.feasible && e.stops.empty() && e.pickDistance == 40.0);
}

TEST(planner_unreachable_and_invalid) {
    Graph g;
    int d = g.addNode("Depot"), p = g.addNode("Packing"), s = g.addNode("Island");
    g.addEdge(d, p, 5);
    RoutePlanner rp(g, d, p);
    CHECK(!rp.planPicking({s}, PickPolicy::NAIVE).feasible);
    CHECK(!rp.planPicking({99}, PickPolicy::NEAREST).feasible);
    CHECK(rp.deliveryDistance(s) == Graph::kInf);
    CHECK(rp.deliveryDistance(99) == Graph::kInf);
    CHECK(rp.deliveryDistance(p) == 0.0);
}

TEST(planner_path_matches_reported_distance) {
    Layout L = gridWarehouse(5, 5);
    RoutePlanner rp(L.graph, L.depot, L.packing);
    std::mt19937 rng(7);
    for (int trial = 0; trial < 30; ++trial) {
        std::vector<int> items;
        int n = 1 + rng() % 8;
        for (int i = 0; i < n; ++i) items.push_back(L.shelfNodes[rng() % L.shelfNodes.size()]);
        for (PickPolicy pol : {PickPolicy::NAIVE, PickPolicy::NEAREST, PickPolicy::NEAREST_2OPT}) {
            RoutePlan plan = rp.planPicking(items, pol);
            CHECK(plan.feasible);
            double walked = 0; bool edgesExist = true;
            for (std::size_t i = 0; i + 1 < plan.path.size(); ++i) {
                double w = L.graph.edgeWeight(plan.path[i], plan.path[i + 1]);
                if (w == Graph::kInf) edgesExist = false; else walked += w;
            }
            CHECK(edgesExist);
            CHECK(std::abs(walked - plan.pickDistance) < 1e-6);   // reported cost == real walk
            CHECK(plan.path.front() == L.depot && plan.path.back() == L.packing);
        }
    }
}

TEST(planner_2opt_never_worse_than_nearest) {
    Layout L = gridWarehouse(6, 6);
    RoutePlanner rp(L.graph, L.depot, L.packing);
    std::mt19937 rng(123);
    int strictlyBetter = 0;
    for (int trial = 0; trial < 60; ++trial) {
        std::vector<int> items;
        int n = 4 + rng() % 6;
        for (int i = 0; i < n; ++i) items.push_back(L.shelfNodes[rng() % L.shelfNodes.size()]);
        double nn = rp.planPicking(items, PickPolicy::NEAREST).pickDistance;
        double opt = rp.planPicking(items, PickPolicy::NEAREST_2OPT).pickDistance;
        CHECK(opt <= nn + 1e-9);
        if (opt < nn - 1e-9) ++strictlyBetter;
    }
    CHECK(strictlyBetter >= 1);                            // 2-opt actually improves some instances
}

