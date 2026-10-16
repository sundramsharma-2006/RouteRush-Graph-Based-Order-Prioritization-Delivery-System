// ===== MEMBER 4 (Ishita Sirohi) : CSV persistence for products and orders =====
// Corrupt/malformed lines are skipped and reported in a LoadReport; loading never crashes.
// Free text is sanitised on save (CSV here has no quoting), so a round trip is lossless for ids,
// numbers and enums and may replace separators inside names by spaces.
#pragma once
#include "Inventory.h"
#include "Types.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct LoadReport {
    int loaded = 0;
    int skipped = 0;
    std::vector<std::string> errors;
    bool fileOpened = true;
};

namespace filestore_detail {
inline std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    std::stringstream ss(s);
    while (std::getline(ss, cur, sep)) out.push_back(cur);
    if (!s.empty() && s.back() == sep) out.push_back("");
    return out;
}
inline std::string clean(std::string s) {
    for (char& c : s) if (c == ',' || c == '\n' || c == '\r' || c == ';' || c == ':') c = ' ';
    return s;
}
inline bool toInt(const std::string& s, int& out) {
    try { std::size_t pos; out = std::stoi(s, &pos); return pos == s.size(); } catch (...) { return false; }
}
inline bool toLong(const std::string& s, long& out) {
    try { std::size_t pos; out = std::stol(s, &pos); return pos == s.size(); } catch (...) { return false; }
}
inline bool toDouble(const std::string& s, double& out) {
    try { std::size_t pos; out = std::stod(s, &pos); return pos == s.size(); } catch (...) { return false; }
}
inline void stripCR(std::string& line) { if (!line.empty() && line.back() == '\r') line.pop_back(); }
}  // namespace filestore_detail

// products.csv : id,name,quantity,node,weightKg
inline bool saveProducts(const Inventory& inv, const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    f << "id,name,quantity,node,weightKg\n";
    inv.forEach([&](const std::string&, const Product& p) {
        f << filestore_detail::clean(p.id) << ',' << filestore_detail::clean(p.name) << ','
          << p.quantity << ',' << p.node << ',' << p.weightKg << '\n';
    });
    return static_cast<bool>(f);
}

inline LoadReport loadProducts(const std::string& path, Inventory& inv) {
    using namespace filestore_detail;
    LoadReport rep;
    std::ifstream f(path);
    if (!f) { rep.fileOpened = false; return rep; }
    std::string line;
    int lineNo = 0;
    while (std::getline(f, line)) {
        ++lineNo;
        stripCR(line);
        if (lineNo == 1 || line.empty()) continue;           // header / blank
        auto c = split(line, ',');
        Product p;
        if (c.size() != 5 || !toInt(c[2], p.quantity) || !toInt(c[3], p.node) || !toDouble(c[4], p.weightKg)) {
            ++rep.skipped; rep.errors.push_back("products line " + std::to_string(lineNo) + ": malformed"); continue;
        }
        p.id = c[0]; p.name = c[1];
        if (!inv.addProduct(p)) {
            ++rep.skipped; rep.errors.push_back("products line " + std::to_string(lineNo) + ": invalid or duplicate"); continue;
        }
        ++rep.loaded;
    }
    return rep;
}

// orders.csv : id,customer,destNode,priority,arrivalTick,status,lines,deadlineTick
// lines = "P1:2;P2:1". The deadline column is optional on load (older files have 7 columns).
inline bool saveOrders(const std::vector<Order>& orders, const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    f << "id,customer,destNode,priority,arrivalTick,status,lines,deadlineTick\n";
    for (const Order& o : orders) {
        f << filestore_detail::clean(o.id) << ',' << filestore_detail::clean(o.customer) << ',' << o.destNode << ','
          << toString(o.priority) << ',' << o.arrivalTick << ',' << toString(o.status) << ',';
        for (std::size_t i = 0; i < o.lines.size(); ++i)
            f << (i ? ";" : "") << filestore_detail::clean(o.lines[i].productId) << ':' << o.lines[i].qty;
        f << ',' << o.deadlineTick << '\n';
    }
    return static_cast<bool>(f);
}

inline LoadReport loadOrders(const std::string& path, std::vector<Order>& out) {
    using namespace filestore_detail;
    LoadReport rep;
    std::ifstream f(path);
    if (!f) { rep.fileOpened = false; return rep; }
    std::string line;
    int lineNo = 0;
    while (std::getline(f, line)) {
        ++lineNo;
        stripCR(line);
        if (lineNo == 1 || line.empty()) continue;
        auto c = split(line, ',');
        Order o;
        bool ok = (c.size() == 7 || c.size() == 8) && toInt(c[2], o.destNode) && parsePriority(c[3], o.priority) &&
                  toLong(c[4], o.arrivalTick) && parseStatus(c[5], o.status) && !c[0].empty();
        if (ok && c.size() == 8) ok = toLong(c[7], o.deadlineTick);
        if (ok) {
            for (const auto& item : split(c[6], ';')) {
                auto kv = split(item, ':');
                OrderLine l;
                if (kv.size() != 2 || !toInt(kv[1], l.qty) || l.qty <= 0 || kv[0].empty()) { ok = false; break; }
                l.productId = kv[0];
                o.lines.push_back(l);
            }
            if (o.lines.empty()) ok = false;
        }
        if (!ok) { ++rep.skipped; rep.errors.push_back("orders line " + std::to_string(lineNo) + ": malformed"); continue; }
        o.id = c[0]; o.customer = c[1];
        out.push_back(o);
        ++rep.loaded;
    }
    return rep;
}
