# RouteRush - Graph-Based Order Prioritization & Delivery System
Team 404 CREW (DSCPP-III-2026-T090) | C++17 | no external libraries
Mentor: Dr. Pawan Kumar Mishra

Order in -> priority queue (aging) -> stock check -> picking route (Dijkstra + heuristics) -> vehicle choice -> delivery log.
**Status: backend, terminal menu and local web frontend (this repo).**

## Build and run
Needs `g++` with C++17 (GCC 8 or newer).
```
make            # builds routerush, routerush_web, run_tests, benchmark   (Windows/MinGW: mingw32-make)
make test       # unit, pipeline, menu and frontend-API tests
make run        # one-shot demo on data/*.csv  (options: --fifo  --pick=naive|nn|2opt)
make menu       # interactive terminal menu
make web        # web frontend on http://localhost:8080 (this computer only)
make bench      # benchmarks vs baselines
```
Windows without make: `run.bat` (modes: demo, test, menu, web, bench).
Without make: `g++ -std=c++17 -Iinclude src/main.cpp -o routerush` (web: `src/server.cpp`, add `-lws2_32` on Windows).

## Frontend
`routerush_web` is a small HTTP server (`include/HttpServer.h`, threads per connection, one mutex around the shared state) that
listens on 127.0.0.1 only. It serves the website in `web/` (plain HTML, CSS and JavaScript) and a JSON API (`include/Api.h`).
Pages: Dashboard, Orders (add, filter, remove), Inventory (stock vs demand, restock), Simulation Lab (pick a policy, run it, watch
pickers and vehicles follow the backend's routes in 3D with a timeline and live events), Compare (priority + aging vs a FIFO
baseline, including what it costs), Warehouse Map (3D graph, node details, Dijkstra route finder) and How it works.
The demo map puts the warehouse at Graphic Era (Deemed to be University), Dehradun, delivering to ISBT Dehradun, Clock Tower and Railway Station (approximate road distances, see `docs/ARCHITECTURE.md`).
Each run replays all orders on a fresh inventory and fleet, so your data is never used up. There is no login: do not expose
the port to a network.

The 3D view uses three.js r128. `web/index.html` loads `web/vendor/three.min.js` if present, otherwise the cdnjs copy. For fully
offline use, download https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js into `web/vendor/`. Without three.js or
WebGL every page still works, only the 3D panels show a notice.

## Who owns what
| Member | Code | Tests / docs |
|---|---|---|
| **Sundram Sharma** (lead) | `include/Types.h`, `Vehicle.h`, `Fleet.h`, `WarehouseManager.h`, `Session.h`, `Api.h`, `HttpServer.h`, `Menu.h`, `src/main.cpp`, `src/server.cpp`, `web/index.html`, `web/css/style.css`, `web/js/{ui,pages,app}.js`, `Makefile`, `run.bat` | `tests/test_fleet.cpp`, `tests/test_manager.cpp`, `tests/test_menu.cpp`, `README.md` |
| **Samriddhi Agarwal** | `include/HashMap.h`, `Inventory.h`, `OrderQueue.h`, `Http.h`, `StaticFiles.h` | `tests/test_hashmap.cpp`, `test_inventory.cpp`, `test_orderqueue.cpp`, `docs/DATA_STRUCTURES.md` |
| **Arya Kumari** | `include/Graph.h`, `WarehouseLayout.h`, `RoutePlanner.h`, `web/js/core.js` (layout, routes), `web/js/scene.js` (3D scene) | `tests/test_graph.cpp`, `test_routeplanner.cpp`, `docs/ARCHITECTURE.md` |
| **Ishita Sirohi** | `include/FileStore.h`, `LayoutStore.h`, `ResultStore.h`, `bench/benchmark.cpp`, `data/` | `tests/test_framework.h`, `test_main.cpp`, `test_filestore.cpp`, `tests/test_api.cpp`, `docs/TESTING.md`, `docs/BENCHMARK_RESULTS.md` |

See `docs/ARCHITECTURE.md` for design and assumptions, `docs/BENCHMARK_RESULTS.md` for measured results and their limits.
