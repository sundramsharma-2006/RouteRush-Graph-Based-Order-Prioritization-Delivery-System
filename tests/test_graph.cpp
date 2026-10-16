// ===== MEMBER 3 (Arya Kumari) =====
#include "Graph.h"
#include "WarehouseLayout.h"
#include "test_framework.h"

TEST(dijkstra_prefers_cheaper_indirect_path) {
    Graph g;
    for (int i = 0; i < 5; ++i) g.addNode("n" + std::to_string(i));
    g.addEdge(0, 1, 4); g.addEdge(0, 2, 1); g.addEdge(2, 1, 2); g.addEdge(1, 3, 1);   // node 4 isolated
    CHECK(g.distance(0, 1) == 3.0);                        // 0-2-1 beats direct 0-1
    CHECK(g.distance(0, 3) == 4.0);
    CHECK((g.path(0, 3) == std::vector<int>{0, 2, 1, 3}));
    CHECK(g.distance(0, 0) == 0.0);
}

TEST(dijkstra_unreachable_and_invalid_inputs) {
    Graph g;
    for (int i = 0; i < 3; ++i) g.addNode("n" + std::to_string(i));
    g.addEdge(0, 1, 1);
    CHECK(g.distance(0, 2) == Graph::kInf);
    CHECK(g.path(0, 2).empty());
    CHECK(!g.addEdge(0, 1, -1));                           // negative weight rejected
    CHECK(!g.addEdge(0, 99, 1));                           // bad node rejected
    CHECK(g.distance(0, 99) == Graph::kInf);
    CHECK(g.distance(-1, 0) == Graph::kInf);
}

TEST(graph_lookup_helpers) {
    Graph g;
    int a = g.addNode("A"), b = g.addNode("B"); g.addNode("C");
    g.addEdge(a, b, 5); g.addEdge(a, b, 3);                // parallel edges: smallest wins
    CHECK(g.findNode("B") == 1 && g.findNode("Z") == -1);
    CHECK(g.edgeWeight(a, b) == 3.0 && g.edgeWeight(b, a) == 3.0);
    CHECK(g.edgeWeight(a, 2) == Graph::kInf);
    CHECK(g.distance(a, b) == 3.0);
}

TEST(layouts_are_connected) {
    Layout d = defaultWarehouse();
    CHECK(d.graph.nodeCount() == 9);
    for (int s : d.shelfNodes) CHECK(d.graph.distance(d.depot, s) < Graph::kInf);
    for (int c : d.destNodes) CHECK(d.graph.distance(d.packing, c) < Graph::kInf);
    Layout gr = gridWarehouse(4, 5);
    CHECK(gr.graph.nodeCount() == 2 + 20 + 3);
    for (int s : gr.shelfNodes) CHECK(gr.graph.distance(gr.depot, s) < Graph::kInf);
    CHECK(gr.graph.distance(gr.depot, gr.packing) == 3.0 + 2.0 * (3 + 4) + 3.0);   // corner to corner
}
