// ===== MEMBER 1 (Sundram Sharma) : pipeline / integration tests =====
#include "WarehouseLayout.h"
#include "WarehouseManager.h"
#include "test_framework.h"

namespace {
struct Rig {
    Layout L = defaultWarehouse();
    Inventory inv;
    Fleet fleet = Fleet::standard(2, 1, 1);
    Rig() {
        inv.addProduct({"P1", "Rice", 10, 1, 5.0});      // ZoneA
        inv.addProduct({"P2", "Laptop", 2, 2, 2.5});     // ZoneB
        inv.addProduct({"P3", "Chair", 4, 3, 12.0});     // ZoneC
        inv.addProduct({"P4", "Water", 100, 4, 8.0});    // ZoneD
    }
    WarehouseManager make(WarehouseManager::Config c = WarehouseManager::Config()) {
        return WarehouseManager(L.graph, inv, fleet, L.depot, L.packing, c);
    }
};
Order ord(const std::string& id, Priority p, long arrive, std::vector<OrderLine> lines, int dest = 6, long deadline = -1) {
    Order o; o.id = id; o.customer = "c" + id; o.priority = p; o.arrivalTick = arrive; o.lines = lines;
    o.destNode = dest; o.deadlineTick = deadline; return o;
}
}  // namespace

TEST(manager_delivers_and_updates_stock) {
    Rig r; auto wm = r.make();
    Order o = ord("A", Priority::NORMAL, 0, {{"P1", 2}, {"P2", 1}});
    long busy = 0;
    DeliveryRecord rec = wm.process(o, 0, busy);
    CHECK(rec.status == OrderStatus::DELIVERED && o.status == OrderStatus::DELIVERED);
    CHECK(r.inv.find("P1")->quantity == 8 && r.inv.find("P2")->quantity == 1);
    CHECK(rec.pickDistance > 0 && rec.deliveryDistance == 20.0);
    CHECK(rec.vehicles == "Van:V1");                       // 2*5 + 2.5 = 12.5 kg exceeds the 10 kg bike
}

TEST(manager_vehicle_matches_weight) {
    Rig r; auto wm = r.make();
    Order small = ord("S", Priority::NORMAL, 0, {{"P2", 1}});            // 2.5 kg -> bike
    Order big = ord("B", Priority::NORMAL, 0, {{"P4", 20}});             // 160 kg -> van
    long busy;
    CHECK(wm.process(small, 0, busy).vehicles.find("Bike") == 0);
    CHECK(wm.process(big, 0, busy).vehicles.find("Van") == 0);
}

TEST(manager_rejections_leave_state_untouched) {
    Rig r; auto wm = r.make();
    long busy = 0;
    Order unknown = ord("U", Priority::NORMAL, 0, {{"NOPE", 1}});
    Order noStock = ord("N", Priority::NORMAL, 0, {{"P1", 1}, {"P2", 99}});
    Order badDest = ord("D", Priority::NORMAL, 0, {{"P1", 1}}, 999);
    Order empty = ord("E", Priority::NORMAL, 0, {});
    Order zeroQty = ord("Z", Priority::NORMAL, 0, {{"P1", 0}});
    CHECK(wm.process(unknown, 0, busy).status == OrderStatus::REJECTED);
    CHECK(wm.process(noStock, 0, busy).note == "insufficient stock");
    CHECK(wm.process(badDest, 0, busy).note == "invalid destination");
    CHECK(wm.process(empty, 0, busy).note == "empty order");
    CHECK(wm.process(zeroQty, 0, busy).note == "invalid quantity");
    CHECK(r.inv.find("P1")->quantity == 10 && r.inv.find("P2")->quantity == 2);   // nothing reserved
    CHECK(busy == 0);
}

TEST(manager_unreachable_destination_rejected) {
    Rig r;
    int island = r.L.graph.addNode("Island");                // added BEFORE planner is built
    auto wm = r.make();
    Order o = ord("I", Priority::NORMAL, 0, {{"P1", 1}}, island);
    long busy;
    CHECK(wm.process(o, 0, busy).note == "destination unreachable");
    CHECK(r.inv.find("P1")->quantity == 10);
}

TEST(manager_overweight_without_big_vehicle_is_rejected_without_reserving) {
    Rig r; r.fleet = Fleet::standard(1, 0, 0);                // only a 10 kg bike
    auto wm = r.make();
    Order o = ord("H", Priority::NORMAL, 0, {{"P4", 10}});    // 80 kg
    long busy;
    CHECK(wm.process(o, 0, busy).note == "no vehicle available for this load");
    CHECK(r.inv.find("P4")->quantity == 100);
}

TEST(simulation_serves_urgent_before_normal_when_both_waiting) {
    Rig r; auto wm = r.make();
    std::vector<Order> orders = {
        ord("first", Priority::NORMAL, 0, {{"P4", 1}}),       // arrives alone, starts at 0
        ord("normal", Priority::NORMAL, 1, {{"P4", 1}}),
        ord("urgent", Priority::URGENT, 2, {{"P4", 1}}),
    };
    wm.simulate(orders);
    const auto& rec = wm.records();
    CHECK(rec.size() == 3);
    CHECK(rec[0].orderId == "first" && rec[1].orderId == "urgent" && rec[2].orderId == "normal");
    CHECK(rec[0].startTick < rec[1].startTick && rec[1].startTick < rec[2].startTick);
}

TEST(simulation_fifo_mode_keeps_arrival_order) {
    Rig r; WarehouseManager::Config c; c.fifo = true; auto wm = r.make(c);
    wm.simulate({ord("first", Priority::NORMAL, 0, {{"P4", 1}}), ord("normal", Priority::NORMAL, 1, {{"P4", 1}}),
                 ord("urgent", Priority::URGENT, 2, {{"P4", 1}})});
    CHECK(wm.records()[1].orderId == "normal" && wm.records()[2].orderId == "urgent");
}

TEST(simulation_idle_clock_jumps_to_next_arrival) {
    Rig r; auto wm = r.make();
    wm.simulate({ord("late", Priority::NORMAL, 500, {{"P4", 1}})});
    CHECK(wm.records()[0].startTick == 500);                  // no waiting when the system is idle
}

TEST(simulation_deadlines_and_metrics) {
    Rig r; auto wm = r.make();
    Metrics m = wm.simulate({
        ord("ok", Priority::URGENT, 0, {{"P4", 1}}, 6, 1000),
        ord("tight", Priority::URGENT, 0, {{"P4", 1}}, 6, 5),     // impossible deadline -> late
        ord("rej", Priority::NORMAL, 0, {{"NOPE", 1}}),
    });
    CHECK(m.delivered == 2 && m.rejected == 1 && m.late == 1);
    CHECK(m.makespan > 0 && m.totalCost > 0 && m.avgPickDistance() > 0);
    CHECK(m.avgWait(Priority::URGENT) >= 0);
}

TEST(simulation_stock_exhaustion_is_consistent) {
    Rig r; auto wm = r.make();
    std::vector<Order> orders;
    for (int i = 0; i < 5; ++i) orders.push_back(ord("L" + std::to_string(i), Priority::NORMAL, i, {{"P2", 1}}));   // stock 2
    Metrics m = wm.simulate(orders);
    CHECK(m.delivered == 2 && m.rejected == 3);
    CHECK(r.inv.find("P2")->quantity == 0);                   // never negative
}

TEST(simulation_vehicle_busy_delays_dispatch) {
    Rig r; r.fleet = Fleet::standard(1, 0, 0);                // a single bike
    auto wm = r.make();
    wm.simulate({ord("a", Priority::NORMAL, 0, {{"P2", 1}}), ord("b", Priority::NORMAL, 0, {{"P2", 1}})});
    const auto& rec = wm.records();
    // second order cannot leave until the bike is back (2 * delivery ticks after first dispatch)
    CHECK(rec[1].completionTick >= rec[0].completionTick + 20);
}
