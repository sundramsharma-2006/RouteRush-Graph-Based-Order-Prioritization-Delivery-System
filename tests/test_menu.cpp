// ===== MEMBER 1 (Sundram Sharma) : menu tests (the menu is driven by a scripted input stream) =====
#include "Menu.h"
#include "WarehouseLayout.h"
#include "test_framework.h"
#include <sstream>

namespace {
struct MenuRig {
    Layout L = defaultWarehouse();
    LayoutInfo li;
    std::vector<Product> catalog;
    std::vector<Order> orders;
    std::istringstream in;
    std::ostringstream out;
    MenuRig(const std::string& script) : in(script) {
        li.depot = L.depot; li.packing = L.packing;
        catalog.push_back({"P1", "Rice", 10, 1, 5.0});       // ZoneA
        catalog.push_back({"P2", "Laptop", 1, 2, 2.5});      // ZoneB, only 1 in stock
    }
    Menu make() {
        Menu m(L.graph, li, catalog, orders, WarehouseManager::Config(), "unused", in, out);
        m.saveResults = false;
        return m;
    }
};
}  // namespace

TEST(menu_add_order_then_run_delivers_it) {
    // 4 = add order: customer, node 6 (CityNorth), priority word (any case), arrival default, deadline default,
    // one line P1 x2, blank ends lines, confirm y.   7 = run.   0 = exit.
    MenuRig r("4\nZara\n6\nurgent\n\n\nP1\n2\n\ny\n7\n0\n");
    Menu m = r.make();
    CHECK(m.run() == 0);
    CHECK(m.orders().size() == 1);
    const Order& o = m.orders()[0];
    CHECK(o.id == "O1" && o.customer == "Zara" && o.destNode == 6 && o.priority == Priority::URGENT);
    CHECK(o.arrivalTick == 0 && o.deadlineTick == -1 && o.lines.size() == 1 && o.lines[0].qty == 2);
    CHECK(m.lastRecords().size() == 1 && m.lastRecords()[0].status == OrderStatus::DELIVERED);
}

TEST(menu_rejects_bad_input_and_asks_again) {
    // bad node "99" then "CitySouth" (by name, any case); bad priority "9" then "2";
    // unknown product "ZZ" then "P1"; quantity "-3" and "abc" then "1"; confirm y
    MenuRig r("4\nAli\n99\ncitysouth\n9\n2\n5\n\nZZ\nP1\n-3\nabc\n1\n\ny\n0\n");
    Menu m = r.make();
    m.run();
    CHECK(m.orders().size() == 1);
    const Order& o = m.orders()[0];
    CHECK(o.destNode == 7 && o.priority == Priority::EXPRESS && o.arrivalTick == 5);
    CHECK(o.lines.size() == 1 && o.lines[0].productId == "P1" && o.lines[0].qty == 1);
    CHECK(r.out.str().find("no such node") != std::string::npos);
    CHECK(r.out.str().find("unknown product") != std::string::npos);
}

TEST(menu_cancelled_order_is_not_added) {
    MenuRig r("4\nBo\n6\n1\n\n\nP1\n1\n\nn\n0\n");
    Menu m = r.make();
    m.run();
    CHECK(m.orders().empty());
}

TEST(menu_input_ending_mid_entry_does_not_hang_or_add) {
    MenuRig r("4\nCy\n6\n1\n");            // stream ends while asking for the arrival tick
    Menu m = r.make();
    CHECK(m.run() == 0);
    CHECK(m.orders().empty());
}

TEST(menu_restock_turns_a_rejection_into_a_delivery) {
    // P2 has 1 unit; order wants 3 -> rejected. Restock +5, run again -> delivered.
    MenuRig r("4\nDev\n6\n3\n\n\nP2\n3\n\ny\n7\n6\nP2\n5\n7\n0\n");
    Menu m = r.make();
    m.run();
    CHECK(m.catalog()[1].quantity == 6);
    CHECK(m.lastRecords().size() == 1 && m.lastRecords()[0].status == OrderStatus::DELIVERED);
    CHECK(r.out.str().find("insufficient stock") != std::string::npos);     // first run's rejection
}

TEST(menu_add_product_validates_and_extends_catalog) {
    // duplicate id P1 -> asked again -> P3; negative-looking weight "0" -> asked again -> 1.5
    MenuRig r("5\nP1\nP3\nChair\n4\nzonec\n0\n1.5\n0\n");
    Menu m = r.make();
    m.run();
    CHECK(m.catalog().size() == 3);
    const Product& p = m.catalog()[2];
    CHECK(p.id == "P3" && p.name == "Chair" && p.quantity == 4 && p.node == 3 && p.weightKg == 1.5);
}

TEST(menu_changes_policies_and_keeps_on_blank) {
    MenuRig r("8\n2\n1\n8\n\n\n0\n");           // FIFO + naive, then keep both
    Menu m = r.make();
    m.run();
    CHECK(m.config().fifo && m.config().pick == PickPolicy::NAIVE);
}

TEST(menu_fifo_vs_priority_changes_the_service_order) {
    // O1 NORMAL arrives first, O2 URGENT right after. Priority mode serves O2 before O1 only once both wait together,
    // so make both arrive at tick 0: priority -> O2 first, FIFO -> O1 first.
    std::string add1 = "4\nA\n6\n1\n0\n\nP1\n1\n\ny\n";
    std::string add2 = "4\nB\n6\n3\n0\n\nP1\n1\n\ny\n";
    MenuRig pr(add1 + add2 + "7\n0\n");
    Menu mp = pr.make(); mp.run();
    MenuRig fr(add1 + add2 + "8\n2\n\n7\n0\n");
    Menu mf = fr.make(); mf.run();
    CHECK(mp.lastRecords().size() == 2 && mp.lastRecords()[0].orderId == "O2");
    CHECK(mf.lastRecords().size() == 2 && mf.lastRecords()[0].orderId == "O1");
}

TEST(menu_run_with_no_orders_and_invalid_choice_are_harmless) {
    MenuRig r("7\nxyz\n0\n");
    Menu m = r.make();
    CHECK(m.run() == 0);
    CHECK(m.lastRecords().empty());
    CHECK(r.out.str().find("No orders to run") != std::string::npos);
    CHECK(r.out.str().find("Invalid choice") != std::string::npos);
}
