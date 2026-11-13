// ===== MEMBER 1 (Sundram Sharma) : web frontend server =====
// usage: routerush_web [dataDir] [--port=8080]
// Serves the website in web/ (html, css, js) and the JSON API on http://localhost:<port> (this computer only).
// Stop with Ctrl+C.
// Build: g++ -std=c++17 -pthread -Iinclude src/server.cpp -o routerush_web   (Windows: add -lws2_32)
#include "Api.h"
#include "HttpServer.h"
#include "LayoutStore.h"
#include "StaticFiles.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream o; o << f.rdbuf(); return o.str();
}

int main(int argc, char** argv) {
    std::string dir = "data";
    int port = 8080;
    for (int i = 1; i < argc; ++i) {
        if (!std::strncmp(argv[i], "--port=", 7)) {
            port = std::atoi(argv[i] + 7);
            if (port < 1024 || port > 65535) { std::cerr << "port must be between 1024 and 65535\n"; return 2; }
        } else if (argv[i][0] != '-') dir = argv[i];
        else { std::cerr << "unknown option " << argv[i] << "\n"; return 2; }
    }

    Graph g;
    LayoutInfo li = loadLayout(dir + "/layout.csv", g);
    if (!li.report.fileOpened || li.depot < 0 || li.packing < 0) {
        std::cerr << "layout.csv missing or has no Depot/Packing node in '" << dir << "'\n";
        return 1;
    }
    Inventory inv;
    std::vector<Order> orders;
    LoadReport pr = loadProducts(dir + "/products.csv", inv);
    LoadReport orr = loadOrders(dir + "/orders.csv", orders);
    if (!pr.fileOpened || !orr.fileOpened) { std::cerr << "products.csv / orders.csv missing in '" << dir << "'\n"; return 1; }
    std::vector<Product> catalog;
    inv.forEach([&](const std::string&, const Product& p) { catalog.push_back(p); });

    const std::string page = "web/index.html";
    if (readFile(page).empty()) { std::cerr << "cannot find " << page << " (run from the project folder)\n"; return 1; }

    Session session(g, li, catalog, orders);
    Api api(session, [&] { return readFile(page); });          // re-read each time: edit the page, just refresh

    HttpServer server;
    std::string err = server.start(port);
    if (!err.empty()) { std::cerr << "cannot start: " << err << "\n"; return 1; }
    std::cout << "RouteRush web UI: http://localhost:" << port << "   (" << catalog.size() << " products, "
              << orders.size() << " orders loaded; Ctrl+C to stop)\n" << std::flush;
    server.serve([&](const HttpRequest& r) {
        if (r.path.rfind("/api/", 0) == 0) return api.handle(r);
        if (r.method != "GET") { HttpResponse h; h.status = 405; h.body = "{\"error\":\"use GET\"}"; return h; }
        return static_files::serve("web", r.path);          // "/" -> web/index.html
    });
    return 0;
}
