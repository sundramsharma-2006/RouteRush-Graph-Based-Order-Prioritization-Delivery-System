// ===== MEMBER 1 (Sundram Sharma) : interactive menu on top of the backend =====
// The menu edits two plain lists (catalog = products, orders) and "Run simulation" replays ALL orders
// against a FRESH inventory and fleet built from them. So a run never corrupts the inputs, and you can
// change a policy or add an order and run again to compare results.
// All input/output goes through the streams passed in, so tests can drive it with a string script.
#pragma once
#include "FileStore.h"
#include "LayoutStore.h"
#include "ResultStore.h"
#include "WarehouseManager.h"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

inline void printReport(std::ostream& os, const WarehouseManager& wm, const Metrics& m, const WarehouseManager::Config& cfg) {
    os << std::fixed << std::setprecision(1);
    os << "\nqueue policy: " << (cfg.fifo ? "FIFO (baseline)" : "priority + aging")
       << " | picking: " << toString(cfg.pick) << "\n\n";
    os << std::left << std::setw(5) << "ID" << std::setw(9) << "Cust" << std::setw(8) << "Prio"
       << std::setw(10) << "Status" << std::setw(7) << "Pick" << std::setw(7) << "Deliv"
       << std::setw(16) << "Vehicles" << std::setw(8) << "Cost" << std::setw(7) << "Start"
       << std::setw(6) << "Done" << "Note\n";
    for (const DeliveryRecord& r : wm.records()) {
        os << std::setw(5) << r.orderId << std::setw(9) << r.customer << std::setw(8) << toString(r.priority)
           << std::setw(10) << toString(r.status) << std::setw(7) << r.pickDistance << std::setw(7) << r.deliveryDistance
           << std::setw(16) << r.vehicles << std::setw(8) << r.cost << std::setw(7) << r.startTick
           << std::setw(6) << r.completionTick << (r.late ? "LATE " : "") << r.note << "\n";
    }
    os << "\ndelivered " << m.delivered << ", rejected " << m.rejected << ", late " << m.late
       << "\navg pick distance " << m.avgPickDistance() << ", total cost " << m.totalCost
       << ", makespan " << m.makespan << ", avg turnaround " << m.avgTurnaround()
       << "\navg wait  URGENT " << m.avgWait(Priority::URGENT) << " | EXPRESS " << m.avgWait(Priority::EXPRESS)
       << " | NORMAL " << m.avgWait(Priority::NORMAL) << " (max NORMAL " << m.maxWait(Priority::NORMAL) << ")\n";
}

class Menu {
public:
    bool saveResults = true;      // write <dir>/out_*.csv after every run (tests switch this off)

    Menu(const Graph& g, const LayoutInfo& li, std::vector<Product> catalog, std::vector<Order> orders,
         WarehouseManager::Config cfg, std::string dir, std::istream& in = std::cin, std::ostream& out = std::cout)
        : g_(g), li_(li), catalog_(std::move(catalog)), orders_(std::move(orders)), cfg_(cfg),
          dir_(std::move(dir)), in_(in), out_(out) {}

    int run() {
        out_ << "\nRouteRush interactive mode\n";
        while (true) {
            printMenu();
            std::string c;
            if (!readLine("Choose: ", c)) break;                 // end of input
            bool alive = true;
            if (c == "1") showLayout();
            else if (c == "2") listProducts();
            else if (c == "3") listOrders();
            else if (c == "4") alive = addOrder();
            else if (c == "5") alive = addProduct();
            else if (c == "6") alive = restock();
            else if (c == "7") runSimulation();
            else if (c == "8") alive = changePolicy();
            else if (c == "9") alive = saveInputs();
            else if (c == "0" || c == "q" || c == "Q") break;
            else out_ << "Invalid choice, enter a number from 0 to 9.\n";
            if (!alive) break;
        }
        out_ << "Bye.\n";
        return 0;
    }

    const std::vector<Product>& catalog() const { return catalog_; }
    const std::vector<Order>& orders() const { return orders_; }
    const WarehouseManager::Config& config() const { return cfg_; }
    const std::vector<DeliveryRecord>& lastRecords() const { return lastRecords_; }
    const Metrics& lastMetrics() const { return lastMetrics_; }

private:
    // ---------- small input helpers (all return false only when the input stream ended) ----------
    static std::string trim(std::string s) {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        std::size_t i = 0;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        return s.substr(i);
    }
    static std::string lower(std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }
    bool readLine(const std::string& prompt, std::string& s) {
        out_ << prompt << std::flush;
        if (!std::getline(in_, s)) return false;
        s = trim(s);
        return true;
    }
    bool askLong(const std::string& prompt, long& v, long lo, long hi, bool hasDef = false, long def = 0) {
        while (true) {
            std::string s;
            if (!readLine(prompt, s)) return false;
            if (s.empty() && hasDef) { v = def; return true; }
            long x;
            if (filestore_detail::toLong(s, x) && x >= lo && x <= hi) { v = x; return true; }
            out_ << "  enter a whole number between " << lo << " and " << hi << ".\n";
        }
    }
    bool askDouble(const std::string& prompt, double& v) {       // strictly positive
        while (true) {
            std::string s;
            if (!readLine(prompt, s)) return false;
            double x;
            if (filestore_detail::toDouble(s, x) && x > 0.0 && x <= 100000.0) { v = x; return true; }
            out_ << "  enter a positive number.\n";
        }
    }
    bool askText(const std::string& prompt, std::string& v) {    // non-empty, CSV-safe
        while (true) {
            std::string s;
            if (!readLine(prompt, s)) return false;
            s = filestore_detail::clean(s);
            s = trim(s);
            if (!s.empty()) { v = s; return true; }
            out_ << "  this cannot be empty.\n";
        }
    }
    bool askYes(const std::string& prompt, bool& yes) {
        std::string s;
        if (!readLine(prompt, s)) return false;
        s = lower(s);
        yes = (s == "y" || s == "yes");
        return true;
    }

    // ---------- lookups ----------
    int resolveNode(const std::string& s) const {
        int id;
        if (filestore_detail::toInt(s, id)) return g_.valid(id) ? id : -1;
        for (int i = 0; i < g_.nodeCount(); ++i) if (lower(g_.name(i)) == lower(s)) return i;
        return -1;
    }
    const Product* findProduct(const std::string& id) const {
        for (const Product& p : catalog_) if (p.id == id) return &p;
        return nullptr;
    }
    bool orderIdExists(const std::string& id) const {
        for (const Order& o : orders_) if (o.id == id) return true;
        return false;
    }
    std::string nodeLabel(int id) const { return g_.valid(id) ? g_.name(id) : "?"; }

    // ---------- views ----------
    void printMenu() {
        out_ << "\n---------------- MENU ----------------\n"
             << " 1  Show warehouse layout\n"
             << " 2  List products\n"
             << " 3  List orders (" << orders_.size() << ")\n"
             << " 4  Add a new order\n"
             << " 5  Add a new product\n"
             << " 6  Restock a product\n"
             << " 7  Run simulation on all orders\n"
             << " 8  Change policies (now: " << (cfg_.fifo ? "FIFO" : "priority+aging") << ", " << toString(cfg_.pick) << ")\n"
             << " 9  Save products/orders to data files\n"
             << " 0  Exit\n";
    }
    void showLayout() {
        out_ << "\n" << std::left << std::setw(5) << "ID" << std::setw(14) << "Node" << std::setw(20) << "Distance from Packing" << "Role\n";
        out_ << std::fixed << std::setprecision(1);
        for (int i = 0; i < g_.nodeCount(); ++i) {
            double d = g_.distance(li_.packing, i);
            out_ << std::setw(5) << i << std::setw(14) << g_.name(i);
            if (d == Graph::kInf) out_ << std::setw(20) << "unreachable";
            else out_ << std::setw(20) << d;
            out_ << (i == li_.depot ? "depot" : i == li_.packing ? "packing" : "") << "\n";
        }
    }
    void listProducts() {
        std::vector<Product> v = catalog_;
        std::sort(v.begin(), v.end(), [](const Product& a, const Product& b) { return a.id < b.id; });
        out_ << "\n" << std::left << std::setw(8) << "ID" << std::setw(16) << "Name" << std::setw(7) << "Qty"
             << std::setw(14) << "Shelf node" << "Weight(kg)\n" << std::fixed << std::setprecision(1);
        for (const Product& p : v)
            out_ << std::setw(8) << p.id << std::setw(16) << p.name << std::setw(7) << p.quantity
                 << std::setw(14) << nodeLabel(p.node) << p.weightKg << "\n";
        if (v.empty()) out_ << "(no products)\n";
    }
    void listOrders() {
        out_ << "\n" << std::left << std::setw(6) << "ID" << std::setw(10) << "Customer" << std::setw(12) << "Dest"
             << std::setw(9) << "Prio" << std::setw(9) << "Arrival" << std::setw(10) << "Deadline" << "Lines\n";
        for (const Order& o : orders_) {
            out_ << std::setw(6) << o.id << std::setw(10) << o.customer << std::setw(12) << nodeLabel(o.destNode)
                 << std::setw(9) << toString(o.priority) << std::setw(9) << o.arrivalTick
                 << std::setw(10) << (o.deadlineTick < 0 ? std::string("-") : std::to_string(o.deadlineTick));
            for (std::size_t i = 0; i < o.lines.size(); ++i) out_ << (i ? ", " : "") << o.lines[i].productId << " x" << o.lines[i].qty;
            out_ << "\n";
        }
        if (orders_.empty()) out_ << "(no orders)\n";
    }

    // ---------- actions ----------
    bool addOrder() {
        Order o;
        int n = static_cast<int>(orders_.size()) + 1;
        do { o.id = "O" + std::to_string(n++); } while (orderIdExists(o.id));
        out_ << "\nNew order " << o.id << "\n";
        if (!askText("  customer name: ", o.customer)) return false;

        showLayout();
        while (true) {
            std::string s;
            if (!readLine("  delivery node (id or name): ", s)) return false;
            int id = resolveNode(s);
            if (id >= 0) { o.destNode = id; break; }
            out_ << "  no such node.\n";
        }
        while (true) {
            std::string s;
            if (!readLine("  priority (1=NORMAL, 2=EXPRESS, 3=URGENT, or the word): ", s)) return false;
            std::string u = s;
            for (char& c : u) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (u == "1") u = "NORMAL"; else if (u == "2") u = "EXPRESS"; else if (u == "3") u = "URGENT";
            if (parsePriority(u, o.priority)) break;
            out_ << "  enter 1, 2, 3, NORMAL, EXPRESS or URGENT.\n";
        }
        long latest = 0;
        for (const Order& x : orders_) latest = std::max(latest, x.arrivalTick);
        long v;
        if (!askLong("  arrival tick [" + std::to_string(latest) + "]: ", v, 0, 1000000000L, true, latest)) return false;
        o.arrivalTick = v;
        if (!askLong("  deadline tick (-1 = none) [-1]: ", v, -1, 1000000000L, true, -1)) return false;
        o.deadlineTick = v;

        out_ << "  add product lines:\n";
        while (true) {
            std::string pid;
            if (!readLine(o.lines.empty() ? "    product id: " : "    product id (blank = done): ", pid)) return false;
            if (pid.empty()) {
                if (o.lines.empty()) { out_ << "    an order needs at least one line.\n"; continue; }
                break;
            }
            const Product* p = findProduct(pid);
            if (!p) { out_ << "    unknown product '" << pid << "' (see option 2).\n"; continue; }
            out_ << "    " << p->name << ", " << p->quantity << " in stock\n";
            if (!askLong("    quantity: ", v, 1, 100000)) return false;
            OrderLine l; l.productId = pid; l.qty = static_cast<int>(v);
            o.lines.push_back(l);
        }
        bool yes = false;
        if (!askYes("  add this order? (y/n): ", yes)) return false;
        if (yes) { orders_.push_back(o); out_ << "  order " << o.id << " added.\n"; }
        else out_ << "  cancelled.\n";
        return true;
    }

    bool addProduct() {
        Product p;
        out_ << "\nNew product\n";
        while (true) {
            if (!askText("  id: ", p.id)) return false;
            if (!findProduct(p.id)) break;
            out_ << "  id already exists.\n";
        }
        if (!askText("  name: ", p.name)) return false;
        long v;
        if (!askLong("  quantity: ", v, 0, 1000000)) return false;
        p.quantity = static_cast<int>(v);
        showLayout();
        while (true) {
            std::string s;
            if (!readLine("  shelf node (id or name): ", s)) return false;
            int id = resolveNode(s);
            if (id >= 0) { p.node = id; break; }
            out_ << "  no such node.\n";
        }
        if (!askDouble("  weight per unit in kg: ", p.weightKg)) return false;
        catalog_.push_back(p);
        out_ << "  product " << p.id << " added.\n";
        return true;
    }

    bool restock() {
        std::string id;
        while (true) {
            if (!readLine("\nproduct id to restock: ", id)) return false;
            if (findProduct(id)) break;
            out_ << "  unknown product.\n";
        }
        long q;
        if (!askLong("  units to add: ", q, 1, 1000000)) return false;
        for (Product& p : catalog_) if (p.id == id) { p.quantity += static_cast<int>(q); out_ << "  " << p.id << " now has " << p.quantity << ".\n"; }
        return true;
    }

    bool changePolicy() {
        out_ << "\nQueue policy now: " << (cfg_.fifo ? "FIFO" : "priority + aging") << "\n";
        long v;
        if (!askLong("  1 = priority + aging, 2 = FIFO (baseline) [keep]: ", v, 1, 2, true, cfg_.fifo ? 2 : 1)) return false;
        cfg_.fifo = (v == 2);
        out_ << "Picking now: " << toString(cfg_.pick) << "\n";
        long cur = cfg_.pick == PickPolicy::NAIVE ? 1 : cfg_.pick == PickPolicy::NEAREST ? 2 : 3;
        if (!askLong("  1 = naive, 2 = nearest-neighbour, 3 = nn + 2-opt [keep]: ", v, 1, 3, true, cur)) return false;
        cfg_.pick = v == 1 ? PickPolicy::NAIVE : v == 2 ? PickPolicy::NEAREST : PickPolicy::NEAREST_2OPT;
        out_ << "Policies updated.\n";
        return true;
    }

    bool saveInputs() {
        bool yes = false;
        if (!askYes("\nThis overwrites " + dir_ + "/products.csv and " + dir_ + "/orders.csv. Continue? (y/n): ", yes)) return false;
        if (!yes) { out_ << "Not saved.\n"; return true; }
        Inventory inv;
        for (const Product& p : catalog_) inv.addProduct(p);
        bool ok = saveProducts(inv, dir_ + "/products.csv") && saveOrders(orders_, dir_ + "/orders.csv");
        out_ << (ok ? "Saved.\n" : "WARNING: could not write the files.\n");
        return true;
    }

    void runSimulation() {
        if (orders_.empty()) { out_ << "\nNo orders to run. Add one with option 4.\n"; return; }
        Inventory inv;
        for (const Product& p : catalog_) inv.addProduct(p);
        Fleet fleet = Fleet::standard(3, 2, 1);
        WarehouseManager wm(g_, inv, fleet, li_.depot, li_.packing, cfg_);
        lastMetrics_ = wm.simulate(orders_);
        lastRecords_ = wm.records();
        printReport(out_, wm, lastMetrics_, cfg_);
        if (saveResults) {
            bool ok = saveRecords(wm.records(), dir_ + "/out_records.csv") &&
                      saveOrders(wm.finishedOrders(), dir_ + "/out_orders.csv") &&
                      saveProducts(inv, dir_ + "/out_products.csv");
            out_ << (ok ? "\nSaved out_records.csv, out_orders.csv, out_products.csv\n"
                        : "\nWARNING: could not write output files\n");
        }
    }

    const Graph& g_;
    LayoutInfo li_;
    std::vector<Product> catalog_;
    std::vector<Order> orders_;
    WarehouseManager::Config cfg_;
    std::string dir_;
    std::istream& in_;
    std::ostream& out_;
    std::vector<DeliveryRecord> lastRecords_;
    Metrics lastMetrics_;
};
