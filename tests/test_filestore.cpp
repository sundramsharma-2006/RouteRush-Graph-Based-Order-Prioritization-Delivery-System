// ===== MEMBER 4 (Ishita Sirohi) =====
#include "FileStore.h"
#include "LayoutStore.h"
#include "ResultStore.h"
#include "WarehouseLayout.h"
#include "test_framework.h"
#include <cstdio>

static void writeFile(const std::string& path, const std::string& text) { std::ofstream f(path); f << text; }

TEST(products_round_trip) {
    const std::string pf = "_t_products.csv";
    Inventory a;
    a.addProduct({"P1", "Pen, blue", 5, 1, 0.1});          // comma in name is sanitised on save
    a.addProduct({"P2", "Cup", 7, 2, 1.5});
    CHECK(saveProducts(a, pf));
    Inventory b;
    LoadReport r = loadProducts(pf, b);
    CHECK(r.loaded == 2 && r.skipped == 0);
    CHECK(b.find("P2") && b.find("P2")->quantity == 7 && b.find("P2")->weightKg == 1.5);
    CHECK(b.find("P1")->name == "Pen  blue");
    std::remove(pf.c_str());
}

TEST(products_corrupt_lines_are_skipped_not_fatal) {
    const std::string pf = "_t_products2.csv";
    writeFile(pf, "id,name,quantity,node,weightKg\nP1,Ok,1,1,1\nP2,Bad,xx,1,1\nP3,Short,1\nP1,Dup,1,1,1\nP4,Neg,-5,1,1\r\nP5,CRLF,2,1,1\r\n");
    Inventory c;
    LoadReport r = loadProducts(pf, c);
    CHECK(r.loaded == 2 && r.skipped == 4);               // P1, P5 load; bad number, short row, duplicate id, negative qty skipped
    CHECK(r.errors.size() == 4);
    CHECK(!loadProducts("does_not_exist.csv", c).fileOpened);
    std::remove(pf.c_str());
}

TEST(orders_round_trip_with_deadline) {
    const std::string of = "_t_orders.csv";
    std::vector<Order> os;
    Order o; o.id = "O1"; o.customer = "Ann"; o.destNode = 6; o.priority = Priority::EXPRESS; o.arrivalTick = 12;
    o.deadlineTick = 99; o.status = OrderStatus::DELIVERED; o.lines = {{"P1", 2}, {"P2", 1}};
    os.push_back(o);
    CHECK(saveOrders(os, of));
    std::vector<Order> back;
    LoadReport r = loadOrders(of, back);
    CHECK(r.loaded == 1 && back[0].lines.size() == 2 && back[0].priority == Priority::EXPRESS);
    CHECK(back[0].arrivalTick == 12 && back[0].deadlineTick == 99 && back[0].status == OrderStatus::DELIVERED);
    std::remove(of.c_str());
}

TEST(orders_old_format_and_bad_rows) {
    const std::string of = "_t_orders2.csv";
    writeFile(of, "id,customer,destNode,priority,arrivalTick,status,lines\n"
                  "O1,A,1,NORMAL,0,PLACED,P1:0\n"          // qty 0
                  "O2,B,1,WHAT,0,PLACED,P1:1\n"            // bad priority
                  "O3,C,1,URGENT,0,PLACED,P1:1\n"          // valid 7-column legacy row
                  "O4,D,1,URGENT,0,PLACED,P1:1,xyz\n"      // bad deadline
                  "O5,E,1,URGENT,0,PLACED,\n");            // no lines
    std::vector<Order> got;
    LoadReport r = loadOrders(of, got);
    CHECK(r.loaded == 1 && r.skipped == 4);
    CHECK(got[0].id == "O3" && got[0].deadlineTick == -1);
    std::remove(of.c_str());
}

TEST(layout_file_matches_builtin_layout) {
    Graph g;
    LayoutInfo li = loadLayout("data/layout.csv", g);
    if (!li.report.fileOpened) li = loadLayout("../data/layout.csv", g);   // run from tests/ or project root
    CHECK(li.report.fileOpened && li.report.skipped == 0);
    Layout d = defaultWarehouse();
    CHECK(g.nodeCount() == d.graph.nodeCount());
    CHECK(li.depot == d.depot && li.packing == d.packing);
    for (int a = 0; a < g.nodeCount(); ++a)
        for (int b = 0; b < g.nodeCount(); ++b) CHECK(g.distance(a, b) == d.graph.distance(a, b));
}

TEST(layout_bad_lines_are_reported) {
    const std::string lf = "_t_layout.csv";
    writeFile(lf, "# comment\nN,Depot\nN,Packing\nN,Depot\nE,Depot,Packing,5\nE,Depot,Nowhere,1\nE,Depot,Packing,-2\nX,what\n");
    Graph g;
    LayoutInfo li = loadLayout(lf, g);
    CHECK(li.report.loaded == 3 && li.report.skipped == 4);     // dup node, unknown node, negative weight, junk
    CHECK(li.depot == 0 && li.packing == 1);
    CHECK(g.distance(0, 1) == 5.0);
    Graph g2; LayoutInfo none = loadLayout("no_such_layout.csv", g2);
    CHECK(!none.report.fileOpened);
    std::remove(lf.c_str());
}

TEST(records_are_saved) {
    const std::string rf = "_t_records.csv";
    DeliveryRecord r; r.orderId = "O1"; r.customer = "Ann, Jr"; r.status = OrderStatus::DELIVERED; r.vehicles = "Van:V1";
    r.cost = 12.5; r.late = true;
    CHECK(saveRecords({r}, rf));
    std::ifstream f(rf); std::string header, row;
    std::getline(f, header); std::getline(f, row);
    CHECK(header.find("orderId") == 0);
    CHECK(row.find("O1,Ann  Jr,NORMAL,DELIVERED") == 0 && row.back() == '1');
    CHECK(!saveRecords({r}, "/nonexistent_dir/x.csv"));
    std::remove(rf.c_str());
}
