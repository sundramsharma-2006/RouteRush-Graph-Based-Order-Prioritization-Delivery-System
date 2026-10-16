// ===== MEMBER 2 (Samriddhi Agarwal) : inventory on top of HashMap =====
#pragma once
#include "HashMap.h"
#include "Types.h"
#include <vector>

class Inventory {
public:
    bool addProduct(const Product& p) {
        if (p.id.empty() || p.quantity < 0 || p.weightKg <= 0.0 || p.node < 0) return false;
        return map_.insert(p.id, p);
    }
    Product* find(const std::string& id) { return map_.find(id); }
    const Product* find(const std::string& id) const { return map_.find(id); }

    bool hasStock(const std::string& id, int qty) const {
        const Product* p = map_.find(id);
        return p && qty > 0 && p->quantity >= qty;
    }
    bool restock(const std::string& id, int qty) {
        Product* p = map_.find(id);
        if (!p || qty <= 0) return false;
        p->quantity += qty;
        return true;
    }
    bool reserve(const std::string& id, int qty) {         // never lets stock go negative
        if (!hasStock(id, qty)) return false;
        map_.find(id)->quantity -= qty;
        return true;
    }
    // All-or-nothing: a multi-line order either reserves every line or changes nothing.
    // Lines for the same product are summed first, so {P1:3, P1:3} against stock 5 correctly fails.
    bool reserveAll(const std::vector<OrderLine>& lines) {
        HashMap<int> need;
        for (const auto& l : lines) {
            if (l.qty <= 0) return false;
            int* cur = need.find(l.productId);
            need.insert(l.productId, (cur ? *cur : 0) + l.qty);
        }
        bool ok = true;
        need.forEach([&](const std::string& id, int q) { if (!hasStock(id, q)) ok = false; });
        if (!ok) return false;
        need.forEach([&](const std::string& id, int q) { reserve(id, q); });
        return true;
    }
    // Total weight of an order; returns -1 if any product is unknown.
    double orderWeight(const std::vector<OrderLine>& lines) const {
        double w = 0;
        for (const auto& l : lines) {
            const Product* p = map_.find(l.productId);
            if (!p) return -1.0;
            w += p->weightKg * l.qty;
        }
        return w;
    }
    std::size_t size() const { return map_.size(); }
    template <typename Fn> void forEach(Fn fn) const { map_.forEach(fn); }

private:
    HashMap<Product> map_;
};
