// ===== MEMBER 3 (Arya Kumari) : ready-made warehouse layouts =====
// defaultWarehouse(): the small 9-node demo map used by main/tests.
// gridWarehouse(r,c): r x c shelf grid, used by benchmarks to stress route planning.
#pragma once
#include "Graph.h"
#include <string>
#include <vector>

struct Layout {
    Graph graph;
    int depot = 0;
    int packing = 0;
    std::vector<int> shelfNodes;   // nodes where products can be stored
    std::vector<int> destNodes;    // customer areas
};

inline Layout defaultWarehouse() {
    Layout L;
    Graph& g = L.graph;
    L.depot = g.addNode("Depot");          // 0
    int a = g.addNode("ZoneA");            // 1
    int b = g.addNode("ZoneB");            // 2
    int c = g.addNode("ZoneC");            // 3
    int d = g.addNode("ZoneD");            // 4
    L.packing = g.addNode("Packing");      // 5
    int north = g.addNode("CityNorth");    // 6
    int south = g.addNode("CitySouth");    // 7
    int east  = g.addNode("CityEast");     // 8
    g.addEdge(L.depot, a, 10); g.addEdge(L.depot, b, 15);
    g.addEdge(a, b, 5);  g.addEdge(a, c, 12);
    g.addEdge(b, d, 8);  g.addEdge(c, d, 6);
    g.addEdge(c, L.packing, 7);  g.addEdge(d, L.packing, 9);
    g.addEdge(L.packing, north, 20);  g.addEdge(L.packing, south, 35);
    g.addEdge(north, east, 25);       g.addEdge(south, east, 15);
    L.shelfNodes = {a, b, c, d};
    L.destNodes = {north, south, east};
    return L;
}

inline Layout gridWarehouse(int rows, int cols, double cell = 2.0) {
    Layout L;
    Graph& g = L.graph;
    L.depot = g.addNode("Depot");          // 0
    L.packing = g.addNode("Packing");      // 1
    std::vector<std::vector<int>> id(rows, std::vector<int>(cols));
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            id[r][c] = g.addNode("S" + std::to_string(r) + "_" + std::to_string(c));
            L.shelfNodes.push_back(id[r][c]);
        }
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            if (c + 1 < cols) g.addEdge(id[r][c], id[r][c + 1], cell);
            if (r + 1 < rows) g.addEdge(id[r][c], id[r + 1][c], cell);
        }
    g.addEdge(L.depot, id[0][0], 3.0);
    g.addEdge(L.packing, id[rows - 1][cols - 1], 3.0);
    const double dist[] = {20.0, 30.0, 40.0};
    for (int i = 0; i < 3; ++i) {
        int city = g.addNode("City" + std::to_string(i));
        g.addEdge(L.packing, city, dist[i]);
        L.destNodes.push_back(city);
    }
    return L;
}
