// ===== MEMBER 1 (Sundram Sharma) : frontend layer tests (Session, HTTP parsing, JSON API) =====
// No sockets are used: requests are built in memory and passed to Api::handle().
#include "Api.h"
#include "StaticFiles.h"
#include "WarehouseLayout.h"
#include "test_framework.h"
#include <cstdio>
#include <fstream>

namespace {
struct ApiRig {
    Layout L = defaultWarehouse();
    LayoutInfo li;
    Session s;
    Api api;
    static LayoutInfo info(const Layout& l) { LayoutInfo i; i.depot = l.depot; i.packing = l.packing; return i; }
    ApiRig()
        : li(info(L)),
          s(L.graph, li, {{"P1", "Rice", 10, 1, 5.0}, {"P2", "Laptop", 1, 2, 2.5}}, {}),
          api(s, [] { return std::string("<html>page</html>"); }) {}
    HttpResponse get(const std::string& path) { HttpRequest r; r.method = "GET"; r.path = path; return api.handle(r); }
    HttpResponse post(const std::string& path, const std::string& body) {
        HttpRequest r; r.method = "POST"; r.path = path; r.body = body; return api.handle(r);
    }
};
bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }
}  // namespace

TEST(http_url_decoding_and_forms) {
    CHECK(urlDecode("a+b%20c%41") == "a b cA");
    CHECK(urlDecode("100%") == "100%");                       // incomplete escape kept as text
    auto f = parseForm("customer=Zara+K&lines=P1%3A2%3BP2%3A1&empty=&x");
    CHECK(f["customer"] == "Zara K" && f["lines"] == "P1:2;P2:1" && f["empty"].empty() && f.count("x") == 1);
    CHECK(parseForm("").empty());
}

TEST(http_request_head_parsing_and_response_format) {
    HttpRequest r;
    CHECK(parseRequestHead("POST /api/order?x=1 HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n", r));
    CHECK(r.method == "POST" && r.path == "/api/order" && r.query == "x=1");
    CHECK(r.headers["content-length"] == "5" && r.headers["host"] == "localhost");
    HttpRequest bad;
    CHECK(!parseRequestHead("garbage", bad));
    CHECK(!parseRequestHead("GET nopath HTTP/1.1\r\n", bad));
    HttpResponse resp; resp.status = 404; resp.body = "{}";
    std::string wire = serialize(resp);
    CHECK(wire.find("HTTP/1.1 404 Not Found\r\n") == 0 && has(wire, "Content-Length: 2\r\n") && wire.substr(wire.size() - 2) == "{}");
}

TEST(json_escaping_and_numbers) {
    CHECK(json::esc("a\"b\\c\n") == "\"a\\\"b\\\\c\\n\"");
    CHECK(json::esc(std::string(1, '\x01')) == "\"\\u0001\"");
    CHECK(json::num(3.14159) == "3.1" && json::num(2.0, 2) == "2.00" && json::num(Graph::kInf) == "null");
}

TEST(session_validates_before_changing_anything) {
    ApiRig r;
    Product p; p.id = "P3"; p.name = "Chair"; p.quantity = 4; p.node = 3; p.weightKg = 12;
    CHECK(r.s.addProduct(p).empty() && r.s.catalog().size() == 3);
    CHECK(!r.s.addProduct(p).empty());                         // duplicate id
    Product bad = p; bad.id = "P4"; bad.weightKg = 0;
    CHECK(!r.s.addProduct(bad).empty());
    bad = p; bad.id = "P5"; bad.node = 99;
    CHECK(!r.s.addProduct(bad).empty());
    CHECK(r.s.catalog().size() == 3);
    CHECK(!r.s.restock("NOPE", 5).empty() && !r.s.restock("P1", 0).empty());
    CHECK(r.s.restock("P1", 5).empty() && r.s.catalog()[0].quantity == 15);
}

TEST(session_run_never_changes_the_inputs) {
    ApiRig r;
    Order o; o.customer = "Zed"; o.destNode = 6; o.lines = {{"P1", 4}};
    std::string id;
    CHECK(r.s.addOrder(o, &id).empty() && id == "O1");
    RunResult a = r.s.run(WarehouseManager::Config());
    RunResult b = r.s.run(WarehouseManager::Config());
    CHECK(a.metrics.delivered == 1 && b.metrics.delivered == 1);         // the second run still has stock
    CHECK(r.s.catalog()[0].quantity == 10);                              // inputs untouched
    CHECK(a.stockAfter[0].id == "P1" && a.stockAfter[0].quantity == 6);
}

TEST(api_state_and_page) {
    ApiRig r;
    HttpResponse page = r.get("/");
    CHECK(page.status == 200 && has(page.contentType, "text/html") && page.body == "<html>page</html>");
    HttpResponse st = r.get("/api/state");
    CHECK(st.status == 200 && has(st.body, "\"nodes\":[") && has(st.body, "\"name\":\"ISBT Dehradun\"") && has(st.body, "\"id\":\"P2\""));
    CHECK(has(st.body, "\"nextOrderId\":\"O1\""));
    CHECK(r.get("/api/nothing").status == 404 && r.get("/elsewhere").status == 404);
    CHECK(r.post("/api/state", "").status == 405 && r.get("/api/order").status == 405);
}

TEST(api_add_order_run_and_delete) {
    ApiRig r;
    HttpResponse a = r.post("/api/order", "customer=Zara&dest=6&priority=urgent&arrival=0&deadline=&lines=P1%3A2");
    CHECK(a.status == 200 && has(a.body, "\"id\":\"O1\""));
    CHECK(r.s.orders().size() == 1 && r.s.orders()[0].priority == Priority::URGENT && r.s.orders()[0].deadlineTick == -1);
    HttpResponse run = r.post("/api/run", "fifo=0&pick=2opt");
    CHECK(run.status == 200 && has(run.body, "\"delivered\":1") && has(run.body, "\"status\":\"DELIVERED\""));
    CHECK(has(run.body, "\"stockAfter\"") && has(run.body, "\"pick\":\"nn+2opt\""));
    CHECK(r.post("/api/order/delete", "id=O1").status == 200 && r.s.orders().empty());
    CHECK(r.post("/api/order/delete", "id=O1").status == 400);
}

TEST(api_rejects_bad_input_with_400_and_changes_nothing) {
    ApiRig r;
    CHECK(r.post("/api/order", "customer=&dest=6&priority=URGENT&lines=P1%3A1").status == 400);        // no customer
    CHECK(r.post("/api/order", "customer=A&dest=99&priority=URGENT&lines=P1%3A1").status == 400);      // bad node
    CHECK(r.post("/api/order", "customer=A&dest=6&priority=SOON&lines=P1%3A1").status == 400);         // bad priority
    CHECK(r.post("/api/order", "customer=A&dest=6&priority=NORMAL&lines=ZZ%3A1").status == 400);       // unknown product
    CHECK(r.post("/api/order", "customer=A&dest=6&priority=NORMAL&lines=P1%3A0").status == 400);       // qty 0
    CHECK(r.post("/api/order", "customer=A&dest=6&priority=NORMAL&lines=P1").status == 400);           // malformed
    CHECK(r.post("/api/order", "customer=A&dest=6&priority=NORMAL&arrival=-5&lines=P1%3A1").status == 400);
    CHECK(r.post("/api/order", "customer=A&dest=abc&priority=NORMAL&lines=P1%3A1").status == 400);
    CHECK(r.s.orders().empty());
    HttpResponse e = r.post("/api/order", "customer=A&dest=99&priority=NORMAL&lines=P1%3A1");
    CHECK(has(e.body, "\"error\":\"unknown delivery node\""));
    CHECK(r.post("/api/run", "pick=fast").status == 400);
    CHECK(r.post("/api/run", "").status == 400);                                   // no orders yet
    CHECK(r.get("/api/compare").status == 400);
    CHECK(r.post("/api/restock", "id=P1&qty=x").status == 400);
    CHECK(r.post("/api/product", "id=P9&name=X&qty=1&node=3&weight=abc").status == 400);
}

TEST(api_customer_text_is_made_csv_and_json_safe) {
    ApiRig r;
    HttpResponse a = r.post("/api/order", "customer=Bob%2C%3B%22x&dest=6&priority=NORMAL&lines=P1%3A1");
    CHECK(a.status == 200);
    CHECK(r.s.orders()[0].customer.find(',') == std::string::npos && r.s.orders()[0].customer.find(';') == std::string::npos);
    HttpResponse st = r.get("/api/state");
    CHECK(has(st.body, "\\\"x"));                                                // the quote is escaped in JSON
}

TEST(api_compare_shows_priority_beating_fifo_for_urgent_orders) {
    ApiRig r;
    // a NORMAL order first, then an URGENT one at the same time: priority serves URGENT first, FIFO does not
    CHECK(r.post("/api/order", "customer=A&dest=6&priority=NORMAL&arrival=0&lines=P1%3A1").status == 200);
    CHECK(r.post("/api/order", "customer=B&dest=6&priority=URGENT&arrival=0&lines=P1%3A1").status == 200);
    HttpResponse c = r.get("/api/compare");
    CHECK(c.status == 200 && has(c.body, "\"runs\":[") && has(c.body, "Priority + aging") && has(c.body, "FIFO + naive"));
    RunResult ours = r.s.run([] { WarehouseManager::Config c2; return c2; }());
    WarehouseManager::Config base; base.fifo = true; base.pick = PickPolicy::NAIVE;
    RunResult fifo = r.s.run(base);
    CHECK(ours.records[0].orderId == "O2" && fifo.records[0].orderId == "O1");
    CHECK(ours.metrics.avgWait(Priority::URGENT) < fifo.metrics.avgWait(Priority::URGENT));
}

TEST(static_files_refuse_unsafe_paths_and_unknown_types) {
    using namespace static_files;
    CHECK(safeUrlPath("/index.html") && safeUrlPath("/css/style.css") && safeUrlPath("/js/app.js"));
    CHECK(!safeUrlPath("") && !safeUrlPath("index.html") && !safeUrlPath("/"));
    CHECK(!safeUrlPath("/../secret.txt") && !safeUrlPath("/css/../../x.css") && !safeUrlPath("//etc/passwd"));
    CHECK(!safeUrlPath("/.git/config") && !safeUrlPath("/js/.hidden.js") && !safeUrlPath("/a b.js") && !safeUrlPath("/a\\b.js"));
    CHECK(!safeUrlPath("/dir/") && !safeUrlPath("/x%2e%2e/y.js"));
    CHECK(mimeFor("/a.css") == "text/css; charset=utf-8" && mimeFor("/a.JS").find("javascript") != std::string::npos);
    CHECK(mimeFor("/a.png") == "image/png" && mimeFor("/a.exe").empty() && mimeFor("/noext").empty() && mimeFor("/a.cpp").empty());
    CHECK(serve("web", "/../Makefile").status == 404);
    CHECK(serve("web", "/definitely-missing.js").status == 404);
    CHECK(serve("web", "/Makefile").status == 404);                       // no extension -> not served
}

TEST(static_files_serve_a_real_file) {
    const std::string dir = "_t_web";
    // create the folder portably through the filesystem layer: just write next to the tests and use "." as root
    std::string name = "_t_static_probe.css";
    { std::ofstream f(name, std::ios::binary); f << "body{color:red}"; }
    HttpResponse r = static_files::serve(".", "/" + name);
    CHECK(r.status == 200 && r.body == "body{color:red}" && r.contentType.find("text/css") == 0);
    (void)dir;
    std::remove(name.c_str());
}

TEST(session_reset_restores_the_loaded_data) {
    ApiRig r;
    Order o; o.customer = "Zed"; o.destNode = 6; o.lines = {{"P1", 1}};
    CHECK(r.s.addOrder(o).empty() && r.s.restock("P1", 5).empty());
    Product p; p.id = "PX"; p.name = "Extra"; p.quantity = 1; p.node = 3; p.weightKg = 1;
    CHECK(r.s.addProduct(p).empty());
    CHECK(r.s.orders().size() == 1 && r.s.catalog().size() == 3);
    CHECK(r.post("/api/reset", "").status == 200);
    CHECK(r.s.orders().empty() && r.s.catalog().size() == 2 && r.s.catalog()[0].quantity == 10);
    CHECK(r.get("/api/reset").status == 405);
}

TEST(api_state_lists_edges_and_special_nodes) {
    ApiRig r;
    HttpResponse st = r.get("/api/state");
    CHECK(has(st.body, "\"edges\":[{\"a\":0,\"b\":1,\"w\":10.0}"));            // Depot - Grocery Aisle, 10
    CHECK(has(st.body, "\"depot\":0") && has(st.body, "\"packing\":5"));
    int count = 0;                                                             // the default map has 13 edges
    for (std::size_t pos = st.body.find("\"w\":"); pos != std::string::npos; pos = st.body.find("\"w\":", pos + 1)) ++count;
    CHECK(count == 13);
}

TEST(run_result_carries_the_walked_routes) {
    ApiRig r;
    CHECK(r.post("/api/order", "customer=A&dest=6&priority=NORMAL&lines=P1%3A1%3B").status == 400);   // trailing ';' is malformed
    CHECK(r.post("/api/order", "customer=A&dest=6&priority=NORMAL&lines=P1%3A1").status == 200);
    CHECK(r.post("/api/order", "customer=B&dest=7&priority=NORMAL&lines=P2%3A9").status == 200);      // only 1 laptop -> rejected
    RunResult res = r.s.run(WarehouseManager::Config());
    CHECK(res.records.size() == 2 && res.routes.size() == 2);
    const RouteInfo& ok1 = res.routes[0];
    CHECK(res.records[0].status == OrderStatus::DELIVERED && ok1.dest == 6);
    CHECK(!ok1.pickPath.empty() && ok1.pickPath.front() == r.L.depot && ok1.pickPath.back() == r.L.packing);
    CHECK(ok1.stops.size() == 1 && ok1.stops[0] == 1);                        // P1 sits on Grocery Aisle (node 1)
    CHECK(!ok1.deliveryPath.empty() && ok1.deliveryPath.front() == r.L.packing && ok1.deliveryPath.back() == 6);
    CHECK(res.records[1].status == OrderStatus::REJECTED && res.routes[1].pickPath.empty() && res.routes[1].dest == -1);
    HttpResponse run = r.post("/api/run", "");
    CHECK(has(run.body, "\"pickPath\":[0,") && has(run.body, "\"delPath\":[5,") && has(run.body, "\"items\":[{\"product\":\"P1\",\"qty\":1}]"));
}
