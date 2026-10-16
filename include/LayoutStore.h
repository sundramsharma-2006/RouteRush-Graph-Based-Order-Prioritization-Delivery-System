// ===== MEMBER 4 (Ishita Sirohi) : load a warehouse graph from a text file =====
// layout.csv lines (blank lines and '#' comments ignored):
//   N,<name>                      declare a node (ids are assigned 0,1,2.. in file order)
//   E,<nameU>,<nameV>,<weight>    undirected edge between two declared nodes
// A layout must contain nodes named "Depot" and "Packing".
#pragma once
#include "FileStore.h"
#include "Graph.h"

struct LayoutInfo {
    LoadReport report;
    int depot = -1;
    int packing = -1;
};

inline LayoutInfo loadLayout(const std::string& path, Graph& g) {
    using namespace filestore_detail;
    LayoutInfo info;
    std::ifstream f(path);
    if (!f) { info.report.fileOpened = false; return info; }
    std::string line;
    int lineNo = 0;
    while (std::getline(f, line)) {
        ++lineNo;
        stripCR(line);
        if (line.empty() || line[0] == '#') continue;
        auto c = split(line, ',');
        bool ok = false;
        if (c.size() == 2 && c[0] == "N" && !c[1].empty() && g.findNode(c[1]) == -1) {
            g.addNode(c[1]); ok = true;
        } else if (c.size() == 4 && c[0] == "E") {
            double w; int u = g.findNode(c[1]), v = g.findNode(c[2]);
            ok = u >= 0 && v >= 0 && toDouble(c[3], w) && g.addEdge(u, v, w);
        }
        if (ok) ++info.report.loaded;
        else { ++info.report.skipped; info.report.errors.push_back("layout line " + std::to_string(lineNo) + ": invalid"); }
    }
    info.depot = g.findNode("Depot");
    info.packing = g.findNode("Packing");
    return info;
}
