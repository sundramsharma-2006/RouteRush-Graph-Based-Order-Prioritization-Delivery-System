// ===== MEMBER 1 (Sundram Sharma) : vehicle fleet + load assignment =====
// assign() is PURE (changes nothing). The caller commits by calling setFreeAt() on each vehicle
// only after the rest of the order was validated, so a failed order never blocks vehicles.
//
// Rule per load chunk:
//   * if remaining weight exceeds every unused vehicle's capacity -> use the biggest unused vehicle
//     for a full chunk and continue (split delivery);
//   * otherwise pick the CHEAPEST (cost/km) unused vehicle that fits and is free at readyTick;
//     if none is free yet, take the capable one that frees up earliest.
#pragma once
#include "Vehicle.h"
#include <memory>
#include <string>
#include <vector>

class Fleet {
public:
    struct Assignment { Vehicle* vehicle; double loadKg; };

    void add(std::unique_ptr<Vehicle> v) { vehicles_.push_back(std::move(v)); }

    static Fleet standard(int bikes, int vans, int trucks) {
        Fleet f;
        for (int i = 1; i <= bikes;  ++i) f.add(std::make_unique<Bike>("B" + std::to_string(i)));
        for (int i = 1; i <= vans;   ++i) f.add(std::make_unique<Van>("V" + std::to_string(i)));
        for (int i = 1; i <= trucks; ++i) f.add(std::make_unique<Truck>("T" + std::to_string(i)));
        return f;
    }

    std::size_t size() const { return vehicles_.size(); }
    double maxCapacity() const {
        double m = 0; for (auto& v : vehicles_) m = std::max(m, v->capacityKg()); return m;
    }

    bool assign(double weightKg, long readyTick, std::vector<Assignment>& out) const {
        out.clear();
        if (weightKg <= 0.0) return false;
        std::vector<const Vehicle*> used;
        double remaining = weightKg;
        while (remaining > 1e-9) {
            double maxUnused = 0.0;
            for (auto& v : vehicles_) if (!isUsed(used, v.get())) maxUnused = std::max(maxUnused, v->capacityKg());
            if (maxUnused <= 0.0) { out.clear(); return false; }     // fleet exhausted
            double load = std::min(remaining, maxUnused);
            Vehicle* pick = choose(load, readyTick, used);
            if (!pick) { out.clear(); return false; }
            out.push_back({pick, load});
            used.push_back(pick);
            remaining -= load;
        }
        return true;
    }

    static std::string describe(const std::vector<Assignment>& a) {
        std::string s;
        for (std::size_t i = 0; i < a.size(); ++i)
            s += (i ? "+" : "") + a[i].vehicle->type() + ":" + a[i].vehicle->id();
        return s;
    }

private:
    static bool isUsed(const std::vector<const Vehicle*>& used, const Vehicle* v) {
        for (auto* u : used) if (u == v) return true;
        return false;
    }
    Vehicle* choose(double load, long ready, const std::vector<const Vehicle*>& used) const {
        Vehicle* bestFree = nullptr; Vehicle* bestSoon = nullptr;
        for (auto& up : vehicles_) {
            Vehicle* v = up.get();
            if (isUsed(used, v) || !v->canCarry(load)) continue;
            if (v->freeAt() <= ready) {
                if (!bestFree || v->costPerKm() < bestFree->costPerKm() ||
                    (v->costPerKm() == bestFree->costPerKm() && v->capacityKg() < bestFree->capacityKg()))
                    bestFree = v;
            } else if (!bestSoon || v->freeAt() < bestSoon->freeAt() ||
                       (v->freeAt() == bestSoon->freeAt() && v->costPerKm() < bestSoon->costPerKm())) {
                bestSoon = v;
            }
        }
        return bestFree ? bestFree : bestSoon;
    }
    std::vector<std::unique_ptr<Vehicle>> vehicles_;
};
