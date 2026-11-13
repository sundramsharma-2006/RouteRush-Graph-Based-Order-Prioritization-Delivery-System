// ===== MEMBER 1 (Sundram Sharma) : application state shared by the frontend =====
// Holds the plain input lists (products, orders). Every change is validated first and returns an error
// text ("" = success); on error nothing changes. run() replays ALL orders against a FRESH inventory and
// fleet, so a run never alters the inputs and different policies can be compared on identical data.
#pragma once
#include "FileStore.h"
#include "LayoutStore.h"
#include "WarehouseManager.h"
#include <algorithm>
#include <string>
#include <vector>

// Where an order's pickers and vehicles actually went (for drawing / animating a run).
struct RouteInfo {
    int dest = -1;
    std::vector<OrderLine> lines;
    std::vector<int> stops;           // shelves in visiting order
    std::vector<int> pickPath;        // depot -> shelves -> packing, node by node
    std::vector<int> deliveryPath;    // packing -> customer, node by node
};

struct RunResult {
    WarehouseManager::Config cfg;
    Metrics metrics;
    std::vector<DeliveryRecord> records;
    std::vector<RouteInfo> routes;        // same length and order as records (empty paths if rejected)
    std::vector<Product> stockAfter;      // stock left after the whole run, sorted by id
};

class Session {
public:
    Session(const Graph& g, LayoutInfo li, std::vector<Product> catalog, std::vector<Order> orders)
        : g_(g), li_(li), catalog_(catalog), orders_(orders), initialCatalog_(std::move(catalog)), initialOrders_(std::move(orders)) {}

    const Graph& graph() const { return g_; }
    const LayoutInfo& layout() const { return li_; }
    const std::vector<Product>& catalog() const { return catalog_; }
    const std::vector<Order>& orders() const { return orders_; }

    // Back to the data that was loaded from the CSV files at start-up.
    void reset() { catalog_ = initialCatalog_; orders_ = initialOrders_; }

    std::string nextOrderId() const {
        int n = static_cast<int>(orders_.size()) + 1;
        std::string id;
        do { id = "O" + std::to_string(n++); } while (hasOrder(id));
        return id;
    }

    std::string addProduct(Product p) {
        p.id = tidy(p.id); p.name = tidy(p.name);
        if (p.id.empty()) return "product id is required";
        if (p.name.empty()) return "product name is required";
        if (findProduct(p.id)) return "product id '" + p.id + "' already exists";
        if (p.quantity < 0 || p.quantity > 1000000) return "quantity must be between 0 and 1000000";
        if (!(p.weightKg > 0.0) || p.weightKg > 100000.0) return "weight must be a positive number of kg";
        if (!g_.valid(p.node)) return "unknown shelf node";
        catalog_.push_back(p);
        return "";
    }

    std::string restock(const std::string& id, long qty) {
        Product* p = findProduct(id);
        if (!p) return "unknown product '" + id + "'";
        if (qty <= 0 || qty > 1000000) return "units to add must be between 1 and 1000000";
        p->quantity += static_cast<int>(qty);
        return "";
    }

    // o.id may be empty (a new id is assigned). Returns "" on success; the assigned id is in `assigned`.
    std::string addOrder(Order o, std::string* assigned = nullptr) {
        o.customer = tidy(o.customer);
        if (o.customer.empty()) return "customer name is required";
        if (!g_.valid(o.destNode)) return "unknown delivery node";
        if (o.arrivalTick < 0) return "arrival tick cannot be negative";
        if (o.deadlineTick < -1) return "deadline must be -1 (none) or a tick";
        if (o.lines.empty()) return "an order needs at least one product line";
        for (const OrderLine& l : o.lines) {
            if (!findProduct(l.productId)) return "unknown product '" + l.productId + "'";
            if (l.qty <= 0 || l.qty > 100000) return "quantity must be between 1 and 100000";
        }
        if (o.id.empty()) o.id = nextOrderId();
        else { o.id = tidy(o.id); if (hasOrder(o.id)) return "order id '" + o.id + "' already exists"; }
        o.status = OrderStatus::PLACED;
        if (assigned) *assigned = o.id;
        orders_.push_back(o);
        return "";
    }

    bool removeOrder(const std::string& id) {
        auto it = std::find_if(orders_.begin(), orders_.end(), [&](const Order& o) { return o.id == id; });
        if (it == orders_.end()) return false;
        orders_.erase(it);
        return true;
    }

    RunResult run(const WarehouseManager::Config& cfg) const {
        Inventory inv;
        for (const Product& p : catalog_) inv.addProduct(p);
        Fleet fleet = Fleet::standard(3, 2, 1);
        WarehouseManager wm(g_, inv, fleet, li_.depot, li_.packing, cfg);
        RunResult r;
        r.cfg = cfg;
        r.metrics = wm.simulate(orders_);
        r.records = wm.records();
        // routes: re-plan each delivered order with the same policy to learn the node-by-node walk
        for (const DeliveryRecord& rec : r.records) {
            RouteInfo ri;
            if (rec.status == OrderStatus::DELIVERED) {
                for (const Order& o : wm.finishedOrders()) {
                    if (o.id != rec.orderId) continue;
                    ri.dest = o.destNode;
                    ri.lines = o.lines;
                    std::vector<int> shelves;
                    for (const OrderLine& l : o.lines)
                        if (const Product* p = inv.find(l.productId)) shelves.push_back(p->node);
                    RoutePlan plan = wm.planner().planPicking(shelves, cfg.pick);
                    if (plan.feasible) { ri.stops = plan.stops; ri.pickPath = plan.path; }
                    ri.deliveryPath = wm.planner().deliveryPath(o.destNode);
                    break;
                }
            }
            r.routes.push_back(ri);
        }
        inv.forEach([&](const std::string&, const Product& p) { r.stockAfter.push_back(p); });
        std::sort(r.stockAfter.begin(), r.stockAfter.end(), [](const Product& a, const Product& b) { return a.id < b.id; });
        return r;
    }

private:
    static std::string tidy(std::string s) {                 // CSV-safe and trimmed
        s = filestore_detail::clean(s);
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        std::size_t i = 0;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        return s.substr(i);
    }
    Product* findProduct(const std::string& id) {
        for (Product& p : catalog_) if (p.id == id) return &p;
        return nullptr;
    }
    bool hasOrder(const std::string& id) const {
        for (const Order& o : orders_) if (o.id == id) return true;
        return false;
    }

    const Graph& g_;
    LayoutInfo li_;
    std::vector<Product> catalog_;
    std::vector<Order> orders_;
    std::vector<Product> initialCatalog_;
    std::vector<Order> initialOrders_;
};
