// ===== MEMBER 4 (Ishita Sirohi) : benchmarks against baselines =====
// Answers "does each data structure / algorithm actually help?" with numbers.
//   1. HashMap lookup vs linear scan vs std::unordered_map           (ns per lookup)
//   2. Queue policy: FIFO vs priority+aging (several aging settings)  (waits, late orders)
//   3. Picking policy: naive vs nearest-neighbour vs NN+2-opt         (distance per order)
// All workloads are synthetic and seeded (mt19937), so results are reproducible.
// Timings (part 1) depend on the machine; parts 2-3 are deterministic.
#include "Inventory.h"
#include "WarehouseLayout.h"
#include "WarehouseManager.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <unordered_map>

using Clock = std::chrono::steady_clock;

static void benchLookup() {
    std::cout << "== 1. product lookup (ns per lookup, lower is better) ==\n";
    std::cout << std::setw(9) << "products" << std::setw(14) << "linear scan" << std::setw(14) << "HashMap"
              << std::setw(18) << "unordered_map" << "\n";
    std::mt19937 rng(1);
    for (int n : {100, 1000, 10000, 100000}) {
        std::vector<Product> vec; HashMap<Product> hm; std::unordered_map<std::string, Product> um;
        for (int i = 0; i < n; ++i) {
            Product p{"P" + std::to_string(i), "x", 10, i % 50, 1.0};
            vec.push_back(p); hm.insert(p.id, p); um[p.id] = p;
        }
        const int Q = n >= 100000 ? 500 : 5000;               // fewer queries for the slow baseline
        std::vector<std::string> keys;
        for (int q = 0; q < Q; ++q) keys.push_back("P" + std::to_string(rng() % n));
        long sink = 0;
        auto t0 = Clock::now();
        for (auto& k : keys) for (auto& p : vec) if (p.id == k) { sink += p.quantity; break; }
        auto t1 = Clock::now();
        for (auto& k : keys) sink += hm.find(k)->quantity;
        auto t2 = Clock::now();
        for (auto& k : keys) sink += um.find(k)->second.quantity;
        auto t3 = Clock::now();
        auto ns = [&](Clock::time_point a, Clock::time_point b) {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count() / static_cast<double>(Q);
        };
        std::cout << std::setw(9) << n << std::setw(14) << std::fixed << std::setprecision(0) << ns(t0, t1)
                  << std::setw(14) << ns(t1, t2) << std::setw(18) << ns(t2, t3) << (sink < 0 ? "?" : "") << "\n";
    }
}

struct Workload {
    Layout layout;
    std::vector<Product> products;
    std::vector<Order> orders;
};

static Workload makeWorkload(unsigned seed, int nOrders, int meanGap, int maxLines, int rows, int cols) {
    Workload w;
    w.layout = gridWarehouse(rows, cols);
    std::mt19937 rng(seed);
    for (int i = 0; i < 200; ++i)
        w.products.push_back({"P" + std::to_string(i), "item", 1000000,
                              w.layout.shelfNodes[rng() % w.layout.shelfNodes.size()], 0.5 + (rng() % 40) / 10.0});
    long t = 0;
    for (int i = 0; i < nOrders; ++i) {
        Order o;
        o.id = "O" + std::to_string(i);
        o.customer = "c";
        t += 1 + rng() % (2 * meanGap);                        // mean gap = meanGap
        o.arrivalTick = t;
        int r = rng() % 100;                                   // 20% urgent, 30% express, 50% normal
        o.priority = r < 20 ? Priority::URGENT : r < 50 ? Priority::EXPRESS : Priority::NORMAL;
        o.deadlineTick = t + (o.priority == Priority::URGENT ? 250 : o.priority == Priority::EXPRESS ? 500 : 1000);
        o.destNode = w.layout.destNodes[rng() % w.layout.destNodes.size()];
        int nl = 1 + rng() % maxLines;
        for (int l = 0; l < nl; ++l) o.lines.push_back({"P" + std::to_string(rng() % 200), 1 + static_cast<int>(rng() % 3)});
        w.orders.push_back(o);
    }
    return w;
}

static Metrics runOnce(const Workload& w, WarehouseManager::Config cfg) {
    Inventory inv;
    for (const Product& p : w.products) inv.addProduct(p);
    Fleet fleet = Fleet::standard(30, 15, 5);                  // plenty of vehicles: isolate the queue/route effects
    WarehouseManager wm(w.layout.graph, inv, fleet, w.layout.depot, w.layout.packing, cfg);
    return wm.simulate(w.orders);
}

static void benchQueue() {
    std::cout << "\n== 2. queue policy (400 orders, 6x6 grid, mean arrival gap 28 ticks, 5 seeds averaged) ==\n";
    std::cout << std::setw(26) << "policy" << std::setw(12) << "wait URG" << std::setw(12) << "wait EXP" << std::setw(12)
              << "wait NORM" << std::setw(12) << "max NORM" << std::setw(10) << "late" << "\n";
    struct Row { const char* name; bool fifo; double aging; };
    for (Row row : {Row{"FIFO (baseline)", true, 0}, Row{"priority, aging=10", false, 10}, Row{"priority, aging=50", false, 50},
                    Row{"priority, aging=200", false, 200}, Row{"strict-ish, aging=5000", false, 5000}}) {
        double wu = 0, we = 0, wn = 0, late = 0; long maxN = 0;
        for (unsigned seed = 1; seed <= 5; ++seed) {
            Workload w = makeWorkload(seed, 400, 28, 4, 6, 6);
            WarehouseManager::Config cfg; cfg.fifo = row.fifo; cfg.agingTicksPerLevel = row.aging ? row.aging : 50;
            Metrics m = runOnce(w, cfg);
            wu += m.avgWait(Priority::URGENT); we += m.avgWait(Priority::EXPRESS); wn += m.avgWait(Priority::NORMAL);
            late += m.late; maxN = std::max(maxN, m.maxWait(Priority::NORMAL));
        }
        std::cout << std::setw(26) << row.name << std::setw(12) << std::fixed << std::setprecision(1) << wu / 5
                  << std::setw(12) << we / 5 << std::setw(12) << wn / 5 << std::setw(12) << maxN
                  << std::setw(10) << late / 5 << "\n";
    }
    std::cout << "(wait = ticks between arrival and start of picking; late = delivered after deadline, avg per run)\n";
}

static void benchPicking() {
    std::cout << "\n== 3. picking route policy (300 orders, 8x8 grid, up to 10 lines, 5 seeds averaged) ==\n";
    std::cout << std::setw(22) << "policy" << std::setw(18) << "avg pick dist" << std::setw(14) << "vs naive"
              << std::setw(16) << "makespan" << std::setw(10) << "late" << "\n";
    double naiveAvg = 0;
    for (PickPolicy pol : {PickPolicy::NAIVE, PickPolicy::NEAREST, PickPolicy::NEAREST_2OPT}) {
        double dist = 0, mk = 0, late = 0;
        for (unsigned seed = 1; seed <= 5; ++seed) {
            Workload w = makeWorkload(seed, 300, 60, 10, 8, 8);
            WarehouseManager::Config cfg; cfg.pick = pol;
            Metrics m = runOnce(w, cfg);
            dist += m.avgPickDistance(); mk += m.makespan; late += m.late;
        }
        dist /= 5; if (pol == PickPolicy::NAIVE) naiveAvg = dist;
        std::cout << std::setw(22) << toString(pol) << std::setw(18) << std::fixed << std::setprecision(1) << dist
                  << std::setw(13) << std::setprecision(1) << (100.0 * (dist - naiveAvg) / naiveAvg) << "%"
                  << std::setw(16) << std::setprecision(0) << mk / 5 << std::setw(10) << std::setprecision(1) << late / 5 << "\n";
    }
    std::cout << "(NN and 2-opt are heuristics: they are not guaranteed optimal.)\n";
}

int main() {
    benchLookup();
    benchQueue();
    benchPicking();
    return 0;
}
