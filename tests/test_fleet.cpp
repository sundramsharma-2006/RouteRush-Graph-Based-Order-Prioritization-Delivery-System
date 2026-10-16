// ===== MEMBER 1 (Sundram Sharma) =====
#include "Fleet.h"
#include "test_framework.h"

TEST(vehicles_polymorphic_attributes) {
    Bike b("B"); Van v("V"); Truck t("T");
    const Vehicle* all[] = {&b, &v, &t};
    CHECK(all[0]->type() == "Bike" && all[1]->type() == "Van" && all[2]->type() == "Truck");
    CHECK(all[1]->canCarry(500) && !all[1]->canCarry(500.1));
    CHECK(t.tripCost(10) == 200.0);
}

TEST(fleet_picks_cheapest_vehicle_that_fits) {
    Fleet f = Fleet::standard(1, 1, 1);
    std::vector<Fleet::Assignment> a;
    CHECK(f.assign(5.0, 0, a) && a.size() == 1 && a[0].vehicle->type() == "Bike");
    CHECK(f.assign(100.0, 0, a) && a.size() == 1 && a[0].vehicle->type() == "Van");
    CHECK(f.assign(2000.0, 0, a) && a.size() == 1 && a[0].vehicle->type() == "Truck");
    CHECK(!f.assign(5001.0, 0, a) || a.size() > 1);         // beyond one truck -> split or fail, never one vehicle
}

TEST(fleet_splits_oversize_load_and_fails_when_exhausted) {
    Fleet f;                                                // 2 vans (500kg each), no truck
    f.add(std::make_unique<Van>("V1")); f.add(std::make_unique<Van>("V2"));
    std::vector<Fleet::Assignment> a;
    CHECK(f.assign(800.0, 0, a) && a.size() == 2);
    CHECK(a[0].loadKg + a[1].loadKg == 800.0);
    CHECK(a[0].vehicle != a[1].vehicle);
    CHECK(!f.assign(1001.0, 0, a) && a.empty());            // capacity of whole fleet exceeded
    CHECK(!f.assign(0.0, 0, a));
    CHECK(!f.assign(-5.0, 0, a));
}

TEST(fleet_prefers_free_vehicle_then_earliest_free) {
    Fleet f = Fleet::standard(2, 0, 0);                     // B1, B2
    std::vector<Fleet::Assignment> a;
    f.assign(5.0, 0, a);
    Vehicle* first = a[0].vehicle;
    first->setFreeAt(100);                                  // first bike is busy until 100
    f.assign(5.0, 10, a);
    CHECK(a[0].vehicle != first);                           // the free bike is chosen
    a[0].vehicle->setFreeAt(50);
    f.assign(5.0, 10, a);                                   // both busy: earliest free (50) is chosen
    CHECK(a[0].vehicle->freeAt() == 50);
}

TEST(fleet_assign_does_not_mutate) {
    Fleet f = Fleet::standard(1, 0, 0);
    std::vector<Fleet::Assignment> a;
    f.assign(5.0, 0, a);
    CHECK(a[0].vehicle->freeAt() == 0);
    CHECK(Fleet::describe(a) == "Bike:B1");
}
