// ===== MEMBER 1 (Sundram Sharma) : the order-to-delivery pipeline =====
// For each order (taken from the priority queue):
//   validate -> plan picking route -> plan vehicles -> reserve stock (all-or-nothing) -> commit -> record.
// Everything that can fail is checked BEFORE stock is reserved or vehicles are booked, so a rejected
// order leaves inventory and fleet exactly as they were.
//
// Time model (logical ticks, 1 distance unit = 1 tick, rounded up):
//   * one picker works orders one at a time: busy for ceil(pickDistance) ticks;
//   * vehicles leave when picking is done (or when they free up), travel packing->customer, and come
//     back: a vehicle is busy for 2*ceil(deliveryDistance) ticks;
//   * cost = 2 * deliveryDistance * costPerKm, summed over the vehicles used.
#pragma once
#include "Fleet.h"
#include "Inventory.h"
#include "OrderQueue.h"
#include "RoutePlanner.h"
#include "Types.h"
#include <algorithm>
#include <cmath>

struct Metrics {
    int delivered = 0, rejected = 0, late = 0;
    double totalPickDistance = 0, totalDeliveryDistance = 0, totalCost = 0;
    long makespan = 0;                       // tick at which the last order completed
    double turnaroundSum = 0;                // sum of (completion - arrival) over delivered
    long waitSum[4] = {0, 0, 0, 0};          // queue wait (start - arrival), indexed by Priority 1..3
    long waitMax[4] = {0, 0, 0, 0};
    int waitCount[4] = {0, 0, 0, 0};

    double avgWait(Priority p) const {
        int i = static_cast<int>(p);
        return waitCount[i] ? static_cast<double>(waitSum[i]) / waitCount[i] : 0.0;
    }
    long maxWait(Priority p) const { return waitMax[static_cast<int>(p)]; }
    double avgTurnaround() const { return delivered ? turnaroundSum / delivered : 0.0; }
    double avgPickDistance() const { return delivered ? totalPickDistance / delivered : 0.0; }
};

struct WarehouseConfig {
    PickPolicy pick = PickPolicy::NEAREST_2OPT;
    bool fifo = false;                       // true: ignore priorities (baseline)
    double agingTicksPerLevel = OrderQueue::kDefaultTicksPerLevel;   // see OrderQueue.h
};

class WarehouseManager {
public:
    using Config = WarehouseConfig;

    WarehouseManager(const Graph& g, Inventory& inv, Fleet& fleet, int depot, int packing, Config cfg = Config())
        : g_(g), inv_(inv), fleet_(fleet), planner_(g, depot, packing), cfg_(cfg) {}

    // Runs the whole workload as an event simulation and returns aggregate metrics.
    Metrics simulate(std::vector<Order> orders) {
        records_.clear(); finished_.clear();
        std::stable_sort(orders.begin(), orders.end(),
                         [](const Order& a, const Order& b) { return a.arrivalTick < b.arrivalTick; });
        OrderQueue queue(cfg_.fifo, cfg_.agingTicksPerLevel);
        std::size_t next = 0;
        long clock = 0;
        while (next < orders.size() || !queue.empty()) {
            if (queue.empty() && orders[next].arrivalTick > clock) clock = orders[next].arrivalTick;
            while (next < orders.size() && orders[next].arrivalTick <= clock) {
                orders[next].status = OrderStatus::QUEUED;
                queue.push(orders[next]);
                ++next;
            }
            Order o = queue.pop();
            long busy = 0;
            DeliveryRecord r = process(o, clock, busy);
            clock += busy;
            records_.push_back(r);
            finished_.push_back(o);
        }
        return computeMetrics();
    }

    // Processes ONE order starting at `start`. `busyTicks` = how long the picker is occupied.
    DeliveryRecord process(Order& o, long start, long& busyTicks) {
        busyTicks = 0;
        DeliveryRecord r;
        r.orderId = o.id; r.customer = o.customer; r.priority = o.priority;
        r.arrivalTick = o.arrivalTick; r.startTick = start; r.completionTick = start;
        r.deadlineTick = o.deadlineTick;
        auto reject = [&](const std::string& why) {
            o.status = OrderStatus::REJECTED; r.status = OrderStatus::REJECTED; r.note = why; return r;
        };

        if (o.lines.empty()) return reject("empty order");
        for (const OrderLine& l : o.lines) if (l.qty <= 0) return reject("invalid quantity");
        if (!g_.valid(o.destNode)) return reject("invalid destination");
        double weight = inv_.orderWeight(o.lines);
        if (weight < 0.0) return reject("unknown product");

        std::vector<int> shelfNodes;
        for (const OrderLine& l : o.lines) shelfNodes.push_back(inv_.find(l.productId)->node);
        RoutePlan plan = planner_.planPicking(shelfNodes, cfg_.pick);
        if (!plan.feasible) return reject("shelf unreachable");
        double deliveryDist = planner_.deliveryDistance(o.destNode);
        if (deliveryDist == Graph::kInf) return reject("destination unreachable");

        long pickTicks = ticks(plan.pickDistance), delTicks = ticks(deliveryDist);
        long ready = start + pickTicks;
        std::vector<Fleet::Assignment> trips;
        if (!fleet_.assign(weight, ready, trips)) return reject("no vehicle available for this load");
        if (!inv_.reserveAll(o.lines)) return reject("insufficient stock");

        // ---- commit (nothing below can fail) ----
        long completion = ready;
        double cost = 0.0;
        for (const Fleet::Assignment& t : trips) {
            long dispatch = std::max(ready, t.vehicle->freeAt());
            completion = std::max(completion, dispatch + delTicks);
            t.vehicle->setFreeAt(dispatch + 2 * delTicks);
            cost += 2.0 * deliveryDist * t.vehicle->costPerKm();
        }
        o.status = OrderStatus::DELIVERED;
        busyTicks = pickTicks;
        r.status = OrderStatus::DELIVERED;
        r.pickDistance = plan.pickDistance; r.deliveryDistance = deliveryDist;
        r.vehicles = Fleet::describe(trips); r.cost = cost;
        r.completionTick = completion;
        r.late = o.deadlineTick >= 0 && completion > o.deadlineTick;
        return r;
    }

    const std::vector<DeliveryRecord>& records() const { return records_; }
    const std::vector<Order>& finishedOrders() const { return finished_; }
    const RoutePlanner& planner() const { return planner_; }

private:
    static long ticks(double d) { return d <= 0 ? 0 : static_cast<long>(std::ceil(d - 1e-9)); }

    Metrics computeMetrics() const {
        Metrics m;
        for (std::size_t i = 0; i < records_.size(); ++i) {
            const DeliveryRecord& r = records_[i];
            if (r.status != OrderStatus::DELIVERED) { ++m.rejected; continue; }
            ++m.delivered;
            if (r.late) ++m.late;
            m.totalPickDistance += r.pickDistance;
            m.totalDeliveryDistance += r.deliveryDistance;
            m.totalCost += r.cost;
            m.makespan = std::max(m.makespan, r.completionTick);
            m.turnaroundSum += static_cast<double>(r.completionTick - r.arrivalTick);
            int p = static_cast<int>(r.priority);
            long wait = r.startTick - r.arrivalTick;
            m.waitSum[p] += wait; ++m.waitCount[p]; m.waitMax[p] = std::max(m.waitMax[p], wait);
        }
        return m;
    }

    const Graph& g_;
    Inventory& inv_;
    Fleet& fleet_;
    RoutePlanner planner_;
    Config cfg_;
    std::vector<DeliveryRecord> records_;
    std::vector<Order> finished_;
};
