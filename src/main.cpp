// ===== MEMBER 1 (Sundram Sharma) : command-line driver for the complete backend =====
// usage: routerush [dataDir] [--fifo] [--pick=naive|nn|2opt]
// Reads  <dataDir>/layout.csv, products.csv, orders.csv
// Writes <dataDir>/out_records.csv, out_orders.csv, out_products.csv
#include "FileStore.h"
#include "LayoutStore.h"
#include "ResultStore.h"
#include "WarehouseLayout.h"
#include "WarehouseManager.h"
#include <cstring>
#include <iomanip>
#include <iostream>

static void printReport(const Graph& g, const WarehouseManager& wm, const Metrics& m, const WarehouseManager::Config& cfg) {
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "\nqueue policy: " << (cfg.fifo ? "FIFO (baseline)" : "priority + aging")
              << " | picking: " << toString(cfg.pick) << "\n\n";
    std::cout << std::left << std::setw(5) << "ID" << std::setw(9) << "Cust" << std::setw(8) << "Prio"
              << std::setw(10) << "Status" << std::setw(7) << "Pick" << std::setw(7) << "Deliv"
              << std::setw(16) << "Vehicles" << std::setw(8) << "Cost" << std::setw(7) << "Start"
              << std::setw(6) << "Done" << "Note\n";
    for (const DeliveryRecord& r : wm.records()) {
        std::cout << std::setw(5) << r.orderId << std::setw(9) << r.customer << std::setw(8) << toString(r.priority)
                  << std::setw(10) << toString(r.status) << std::setw(7) << r.pickDistance << std::setw(7) << r.deliveryDistance
                  << std::setw(16) << r.vehicles << std::setw(8) << r.cost << std::setw(7) << r.startTick
                  << std::setw(6) << r.completionTick << (r.late ? "LATE " : "") << r.note << "\n";
    }
    std::cout << "\ndelivered " << m.delivered << ", rejected " << m.rejected << ", late " << m.late
              << "\navg pick distance " << m.avgPickDistance() << ", total cost " << m.totalCost
              << ", makespan " << m.makespan << ", avg turnaround " << m.avgTurnaround()
              << "\navg wait  URGENT " << m.avgWait(Priority::URGENT) << " | EXPRESS " << m.avgWait(Priority::EXPRESS)
              << " | NORMAL " << m.avgWait(Priority::NORMAL) << " (max NORMAL " << m.maxWait(Priority::NORMAL) << ")\n";
    (void)g;
}

int main(int argc, char** argv) {
    std::string dir = "data";
    WarehouseManager::Config cfg;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--fifo")) cfg.fifo = true;
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

    Fleet fleet = Fleet::standard(3, 2, 1);
    WarehouseManager wm(g, inv, fleet, li.depot, li.packing, cfg);
    Metrics m = wm.simulate(orders);
    printReport(g, wm, m, cfg);

    bool saved = saveRecords(wm.records(), dir + "/out_records.csv") &&
                 saveOrders(wm.finishedOrders(), dir + "/out_orders.csv") &&
                 saveProducts(inv, dir + "/out_products.csv");
    std::cout << (saved ? "\nSaved out_records.csv, out_orders.csv, out_products.csv\n" : "\nWARNING: could not write output files\n");
    return saved ? 0 : 1;
}
