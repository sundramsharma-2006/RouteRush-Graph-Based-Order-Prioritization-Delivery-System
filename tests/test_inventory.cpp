// ===== MEMBER 2 (Samriddhi Agarwal) =====
#include "Inventory.h"
#include "test_framework.h"

TEST(inventory_validation) {
    Inventory inv;
    CHECK(inv.addProduct({"P1", "Pen", 5, 1, 0.1}));
    CHECK(!inv.addProduct({"P1", "Dup", 5, 1, 0.1}));      // duplicate id
    CHECK(!inv.addProduct({"P2", "Bad", -1, 1, 0.1}));     // negative qty
    CHECK(!inv.addProduct({"P3", "Heavy0", 1, 1, 0.0}));   // zero weight
    CHECK(!inv.addProduct({"", "NoId", 1, 1, 0.1}));
    CHECK(inv.size() == 1);
}

TEST(inventory_stock_never_negative) {
    Inventory inv;
    inv.addProduct({"P1", "Pen", 5, 1, 0.1});
    CHECK(inv.reserve("P1", 5));
    CHECK(!inv.reserve("P1", 1));
    CHECK(inv.find("P1")->quantity == 0);
    CHECK(!inv.reserve("P1", 0));                          // zero/negative requests rejected
    CHECK(!inv.reserve("P1", -3));
    CHECK(inv.restock("P1", 3) && inv.find("P1")->quantity == 3);
    CHECK(!inv.restock("NOPE", 3));
}

TEST(inventory_reserve_all_is_atomic) {
    Inventory inv;
    inv.addProduct({"P1", "Pen", 3, 1, 0.1});
    inv.addProduct({"P3", "Cup", 5, 1, 1.0});
    CHECK(!inv.reserveAll({{"P1", 1}, {"P3", 99}}));       // 2nd line fails -> 1st untouched
    CHECK(inv.find("P1")->quantity == 3);
    CHECK(!inv.reserveAll({{"P3", 3}, {"P3", 3}}));        // duplicate lines are summed (6 > 5)
    CHECK(inv.find("P3")->quantity == 5);
    CHECK(!inv.reserveAll({{"NOPE", 1}}));
    CHECK(!inv.reserveAll({{"P1", 0}}));
    CHECK(inv.reserveAll({{"P1", 1}, {"P3", 2}}));
    CHECK(inv.find("P1")->quantity == 2 && inv.find("P3")->quantity == 3);
}

TEST(inventory_order_weight) {
    Inventory inv;
    inv.addProduct({"P3", "Cup", 5, 1, 1.5});
    CHECK(inv.orderWeight({{"P3", 2}}) == 3.0);
    CHECK(inv.orderWeight({{"P3", 1}, {"P3", 1}}) == 3.0);
    CHECK(inv.orderWeight({{"NOPE", 1}}) < 0);
}
