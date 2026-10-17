// ===== MEMBER 1 (Sundram Sharma) : command-line driver for the complete backend =====
// usage: routerush [dataDir] [--menu] [--fifo] [--pick=naive|nn|2opt]
//   (no --menu)  batch mode: run every order in orders.csv once and print the report
//   --menu       interactive mode: add orders/products, change policies, run, compare
// Reads  <dataDir>/layout.csv, products.csv, orders.csv
// Writes <dataDir>/out_records.csv, out_orders.csv, out_products.csv
#include "FileStore.h"
#include "LayoutStore.h"
#include "Menu.h"
#include "ResultStore.h"
#include "WarehouseLayout.h"
#include "WarehouseManager.h"
#include <algorithm>
#include <cstring>
#include <iostream>

int main(int argc, char** argv) {
    std::string dir = "data";
    bool interactive = false;
    WarehouseManager::Config cfg;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--fifo")) cfg.fifo = true;
        else if (!std::strcmp(argv[i], "--menu")) interactive = true;
        else if (!std::strcmp(argv[i], "--pick=naive")) cfg.pick = PickPolicy::NAIVE;
        else if (!std::strcmp(argv[i], "--pick=nn")) cfg.pick = PickPolicy::NEAREST;
        else if (!std::strcmp(argv[i], "--pick=2opt")) cfg.pick = PickPolicy::NEAREST_2OPT;
        else if (argv[i][0] != '-') dir = argv[i];
        else { std::cerr << "unknown option " << argv[i] << "\n"; return 2; }
    }

    Graph g;
    LayoutInfo li = loadLayout(dir + "/layout.csv", g);
    if (!li.report.fileOpened || li.depot < 0 || li.packing < 0) {
        std::cerr << "layout.csv missing or has no Depot/Packing node in '" << dir << "'\n";
        return 1;
    }
    Inventory inv;
    std::vector<Order> orders;
    LoadReport pr = loadProducts(dir + "/products.csv", inv);
    LoadReport orr = loadOrders(dir + "/orders.csv", orders);
    if (!pr.fileOpened || !orr.fileOpened) { std::cerr << "products.csv / orders.csv missing in '" << dir << "'\n"; return 1; }
    std::cout << "Loaded layout (" << g.nodeCount() << " nodes), " << pr.loaded << " products, " << orr.loaded << " orders; skipped "
              << li.report.skipped + pr.skipped + orr.skipped << " bad line(s)\n";
    for (auto* rep : {&li.report, &pr, &orr}) for (const auto& e : rep->errors) std::cout << "  warning: " << e << "\n";

    if (interactive) {
        std::vector<Product> catalog;
        inv.forEach([&](const std::string&, const Product& p) { catalog.push_back(p); });
        std::sort(catalog.begin(), catalog.end(), [](const Product& a, const Product& b) { return a.id < b.id; });
        Menu menu(g, li, catalog, orders, cfg, dir);
        return menu.run();
    }

    Fleet fleet = Fleet::standard(3, 2, 1);
    WarehouseManager wm(g, inv, fleet, li.depot, li.packing, cfg);
    Metrics m = wm.simulate(orders);
    printReport(std::cout, wm, m, cfg);

    bool saved = saveRecords(wm.records(), dir + "/out_records.csv") &&
                 saveOrders(wm.finishedOrders(), dir + "/out_orders.csv") &&
                 saveProducts(inv, dir + "/out_products.csv");
    std::cout << (saved ? "\nSaved out_records.csv, out_orders.csv, out_products.csv\n" : "\nWARNING: could not write output files\n");
    return saved ? 0 : 1;
}
