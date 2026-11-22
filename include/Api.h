// ===== MEMBER 1 (Sundram Sharma) : JSON API used by the web frontend =====
// handle() maps a parsed HTTP request to a response; it knows nothing about sockets, so it is unit-tested.
//   GET  /                  the web page          GET  /api/state    nodes, products, orders, next order id
//   POST /api/order         add an order          POST /api/order/delete   remove an order
//   POST /api/product       add a product         POST /api/restock        add stock
//   POST /api/reset         back to the CSV data  (run/compare also return each order's node-by-node routes)
//   POST /api/run           run one policy        GET  /api/compare        priority+aging vs FIFO baseline
// POST bodies are application/x-www-form-urlencoded; responses are JSON ({"error": "..."} with status 400 on bad input).
#pragma once
#include "Http.h"
#include "Session.h"
#include <cstdio>
#include <functional>
#include <iomanip>
#include <sstream>

namespace json {
inline std::string esc(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += static_cast<char>(c);
        }
    }
    return o + "\"";
}
inline std::string num(double d, int prec = 1) {
    if (d != d || d == Graph::kInf) return "null";
    std::ostringstream o; o << std::fixed << std::setprecision(prec) << d; return o.str();
}
}  // namespace json

class Api {
public:
    using IndexLoader = std::function<std::string()>;
    Api(Session& s, IndexLoader index) : s_(s), index_(std::move(index)) {}

    HttpResponse handle(const HttpRequest& r) {
        try { return route(r); }
        catch (const std::exception& e) { return fail(500, std::string("internal error: ") + e.what()); }
    }

    static std::string runJson(const RunResult& r, const std::string& name = "") {
        const Metrics& m = r.metrics;
        std::ostringstream o;
        o << "{";
        if (!name.empty()) o << "\"name\":" << json::esc(name) << ",";
        o << "\"policy\":{\"fifo\":" << (r.cfg.fifo ? "true" : "false") << ",\"pick\":" << json::esc(toString(r.cfg.pick)) << "},";
        o << "\"metrics\":{\"delivered\":" << m.delivered << ",\"rejected\":" << m.rejected << ",\"late\":" << m.late
          << ",\"avgPick\":" << json::num(m.avgPickDistance()) << ",\"totalCost\":" << json::num(m.totalCost)
          << ",\"makespan\":" << m.makespan << ",\"avgTurnaround\":" << json::num(m.avgTurnaround()) << ",\"wait\":{";
        const Priority ps[] = {Priority::URGENT, Priority::EXPRESS, Priority::NORMAL};
        for (int i = 0; i < 3; ++i)
            o << (i ? "," : "") << json::esc(toString(ps[i])) << ":{\"avg\":" << json::num(m.avgWait(ps[i]))
              << ",\"max\":" << m.maxWait(ps[i]) << "}";
        o << "}},\"records\":[";
        for (std::size_t i = 0; i < r.records.size(); ++i) {
            const DeliveryRecord& d = r.records[i];
            o << (i ? "," : "") << "{\"id\":" << json::esc(d.orderId) << ",\"customer\":" << json::esc(d.customer)
              << ",\"priority\":" << json::esc(toString(d.priority)) << ",\"status\":" << json::esc(toString(d.status))
              << ",\"pick\":" << json::num(d.pickDistance) << ",\"deliv\":" << json::num(d.deliveryDistance)
              << ",\"vehicles\":" << json::esc(d.vehicles) << ",\"cost\":" << json::num(d.cost)
              << ",\"arrival\":" << d.arrivalTick << ",\"start\":" << d.startTick << ",\"done\":" << d.completionTick
              << ",\"deadline\":" << d.deadlineTick << ",\"late\":" << (d.late ? "true" : "false")
              << ",\"note\":" << json::esc(d.note);
            if (i < r.routes.size()) {
                const RouteInfo& ri = r.routes[i];
                o << ",\"dest\":" << ri.dest << ",\"items\":[";
                for (std::size_t k = 0; k < ri.lines.size(); ++k)
                    o << (k ? "," : "") << "{\"product\":" << json::esc(ri.lines[k].productId) << ",\"qty\":" << ri.lines[k].qty << "}";
                o << "],\"stops\":" << ints(ri.stops) << ",\"pickPath\":" << ints(ri.pickPath) << ",\"delPath\":" << ints(ri.deliveryPath);
            }
            o << "}";
        }
        o << "],\"stockAfter\":[";
        for (std::size_t i = 0; i < r.stockAfter.size(); ++i)
            o << (i ? "," : "") << "{\"id\":" << json::esc(r.stockAfter[i].id) << ",\"name\":" << json::esc(r.stockAfter[i].name)
              << ",\"qty\":" << r.stockAfter[i].quantity << "}";
        o << "]}";
        return o.str();
    }

private:
    using Form = std::map<std::string, std::string>;

    static std::string ints(const std::vector<int>& v) {
        std::string o = "[";
        for (std::size_t i = 0; i < v.size(); ++i) o += (i ? "," : "") + std::to_string(v[i]);
        return o + "]";
    }

    static HttpResponse ok(const std::string& body) { HttpResponse r; r.body = body; return r; }
    static HttpResponse fail(int status, const std::string& msg) {
        HttpResponse r; r.status = status; r.body = "{\"error\":" + json::esc(msg) + "}"; return r;
    }
    static std::string get(const Form& f, const std::string& k) { auto it = f.find(k); return it == f.end() ? "" : it->second; }
    static bool asLong(const std::string& s, long& v) { return filestore_detail::toLong(s, v); }

    HttpResponse route(const HttpRequest& r) {
        if (r.path == "/" || r.path == "/index.html") {
            if (r.method != "GET") return fail(405, "use GET");
            HttpResponse h; h.contentType = "text/html; charset=utf-8"; h.body = index_(); return h;
        }
        if (r.path.rfind("/api/", 0) != 0) return fail(404, "not found");
        const bool isPost = r.method == "POST";
        if (r.path == "/api/state")  return r.method == "GET" ? ok(stateJson()) : fail(405, "use GET");
        if (r.path == "/api/compare") return r.method == "GET" ? compare() : fail(405, "use GET");
        static const char* const postPaths[] = {"/api/order", "/api/order/delete", "/api/product", "/api/restock", "/api/run", "/api/reset"};
        bool known = false;
        for (const char* p : postPaths) if (r.path == p) known = true;
        if (!known) return fail(404, "not found");
        if (!isPost) return fail(405, "use POST");
        Form f = parseForm(r.body);
        if (r.path == "/api/order") return addOrder(f);
        if (r.path == "/api/order/delete") {
            return s_.removeOrder(get(f, "id")) ? ok("{\"ok\":true}") : fail(400, "no such order");
        }
        if (r.path == "/api/reset") { s_.reset(); return ok("{\"ok\":true}"); }
        if (r.path == "/api/product") return addProduct(f);
        if (r.path == "/api/restock") {
            long q;
            if (!asLong(get(f, "qty"), q)) return fail(400, "units to add must be a whole number");
            std::string e = s_.restock(get(f, "id"), q);
            return e.empty() ? ok("{\"ok\":true}") : fail(400, e);
        }
        if (r.path == "/api/run") return run(f);
        return fail(404, "not found");
    }

    HttpResponse addOrder(const Form& f) {
        Order o;
        o.customer = get(f, "customer");
        long v;
        if (!asLong(get(f, "dest"), v)) return fail(400, "delivery node must be a node id");
        o.destNode = static_cast<int>(v);
        std::string pr = get(f, "priority");
        for (char& c : pr) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (!parsePriority(pr, o.priority)) return fail(400, "priority must be NORMAL, EXPRESS or URGENT");
        o.arrivalTick = asLong(get(f, "arrival"), v) ? v : (get(f, "arrival").empty() ? 0 : -1);
        o.deadlineTick = asLong(get(f, "deadline"), v) ? v : (get(f, "deadline").empty() ? -1 : -2);
        for (const std::string& item : filestore_detail::split(get(f, "lines"), ';')) {     // "P001:2;P003:1"
            auto kv = filestore_detail::split(item, ':');
            long q;
            if (kv.size() != 2 || kv[0].empty() || !asLong(kv[1], q)) return fail(400, "lines must look like P001:2;P003:1");
            OrderLine l; l.productId = kv[0]; l.qty = static_cast<int>(std::max(-1L, std::min(q, 1000000L)));
            o.lines.push_back(l);
        }
        std::string id;
        std::string e = s_.addOrder(o, &id);
        return e.empty() ? ok("{\"ok\":true,\"id\":" + json::esc(id) + "}") : fail(400, e);
    }

    HttpResponse addProduct(const Form& f) {
        Product p;
        p.id = get(f, "id"); p.name = get(f, "name");
        long q, n;
        if (!asLong(get(f, "qty"), q)) return fail(400, "quantity must be a whole number");
        if (!asLong(get(f, "node"), n)) return fail(400, "shelf node must be a node id");
        if (!filestore_detail::toDouble(get(f, "weight"), p.weightKg)) return fail(400, "weight must be a number");
        p.quantity = static_cast<int>(std::max(-1L, std::min(q, 100000000L)));
        p.node = static_cast<int>(n);
        std::string e = s_.addProduct(p);
        return e.empty() ? ok("{\"ok\":true}") : fail(400, e);
    }

    HttpResponse run(const Form& f) {
        WarehouseManager::Config cfg;
        cfg.fifo = get(f, "fifo") == "1" || get(f, "fifo") == "true";
        std::string pk = get(f, "pick");
        if (pk == "naive") cfg.pick = PickPolicy::NAIVE;
        else if (pk == "nn") cfg.pick = PickPolicy::NEAREST;
        else if (pk == "2opt" || pk.empty()) cfg.pick = PickPolicy::NEAREST_2OPT;
        else return fail(400, "pick must be naive, nn or 2opt");
        if (s_.orders().empty()) return fail(400, "there are no orders to run");
        return ok(runJson(s_.run(cfg)));
    }

    HttpResponse compare() {
        if (s_.orders().empty()) return fail(400, "there are no orders to run");
        WarehouseManager::Config a, b;                       // a = our policy, b = baseline
        a.fifo = false; a.pick = PickPolicy::NEAREST_2OPT;
        b.fifo = true;  b.pick = PickPolicy::NAIVE;
        return ok("{\"runs\":[" + runJson(s_.run(a), "Priority + aging, nn+2opt") + "," +
                  runJson(s_.run(b), "FIFO + naive picking (baseline)") + "]}");
    }

    std::string stateJson() const {
        const Graph& g = s_.graph();
        std::ostringstream o;
        o << "{\"nodes\":[";
        for (int i = 0; i < g.nodeCount(); ++i)
            o << (i ? "," : "") << "{\"id\":" << i << ",\"name\":" << json::esc(g.name(i))
              << ",\"dist\":" << json::num(g.distance(s_.layout().packing, i)) << ",\"role\":"
              << json::esc(i == s_.layout().depot ? "depot" : i == s_.layout().packing ? "packing" : "") << "}";
        o << "],\"edges\":[";
        bool firstEdge = true;
        for (int a = 0; a < g.nodeCount(); ++a)
            for (int b = a + 1; b < g.nodeCount(); ++b) {
                double w = g.edgeWeight(a, b);
                if (w == Graph::kInf) continue;
                o << (firstEdge ? "" : ",") << "{\"a\":" << a << ",\"b\":" << b << ",\"w\":" << json::num(w) << "}";
                firstEdge = false;
            }
        o << "],\"depot\":" << s_.layout().depot << ",\"packing\":" << s_.layout().packing << ",\"products\":[";
        std::vector<Product> ps = s_.catalog();
        std::sort(ps.begin(), ps.end(), [](const Product& a, const Product& b) { return a.id < b.id; });
        for (std::size_t i = 0; i < ps.size(); ++i)
            o << (i ? "," : "") << "{\"id\":" << json::esc(ps[i].id) << ",\"name\":" << json::esc(ps[i].name)
              << ",\"qty\":" << ps[i].quantity << ",\"node\":" << ps[i].node << ",\"weight\":" << json::num(ps[i].weightKg, 2) << "}";
        o << "],\"orders\":[";
        for (std::size_t i = 0; i < s_.orders().size(); ++i) {
            const Order& d = s_.orders()[i];
            o << (i ? "," : "") << "{\"id\":" << json::esc(d.id) << ",\"customer\":" << json::esc(d.customer)
              << ",\"dest\":" << d.destNode << ",\"priority\":" << json::esc(toString(d.priority))
              << ",\"arrival\":" << d.arrivalTick << ",\"deadline\":" << d.deadlineTick << ",\"lines\":[";
            for (std::size_t k = 0; k < d.lines.size(); ++k)
                o << (k ? "," : "") << "{\"product\":" << json::esc(d.lines[k].productId) << ",\"qty\":" << d.lines[k].qty << "}";
            o << "]}";
        }
        o << "],\"nextOrderId\":" << json::esc(s_.nextOrderId()) << "}";
        return o.str();
    }

    Session& s_;
    IndexLoader index_;
};
