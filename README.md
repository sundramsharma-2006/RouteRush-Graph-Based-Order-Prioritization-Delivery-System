# RouteRush - Graph-Based Order Prioritization & Delivery System
Team 404 CREW (DSCPP-III-2026-T090) | C++17 | no external libraries

Order in -> priority queue (aging) -> stock check -> picking route (Dijkstra + heuristics) -> vehicle choice -> delivery log.
**Status: backend complete (this repo). Frontend/UI: not started (Phase III).**

## Build and run
```
make            # builds routerush, run_tests, benchmark   (Windows/MinGW: mingw32-make)
make test       # 44 tests
make run        # demo on data/*.csv  (options: --fifo  --pick=naive|nn|2opt)
make bench      # benchmarks vs baselines
```
Without make: `g++ -std=c++17 -Iinclude src/main.cpp -o routerush`

## Who owns what
| Member | Code | Tests / docs |
|---|---|---|
| **Sundram Sharma** (lead) | `include/Types.h`, `Vehicle.h`, `Fleet.h`, `WarehouseManager.h`, `src/main.cpp`, `Makefile` | `tests/test_fleet.cpp`, `tests/test_manager.cpp`, `README.md` |
| **Samriddhi Agarwal** | `include/HashMap.h`, `Inventory.h`, `OrderQueue.h` | `tests/test_hashmap.cpp`, `test_inventory.cpp`, `test_orderqueue.cpp`, `docs/DATA_STRUCTURES.md` |
| **Arya Kumari** | `include/Graph.h`, `WarehouseLayout.h`, `RoutePlanner.h` | `tests/test_graph.cpp`, `test_routeplanner.cpp`, `docs/ARCHITECTURE.md` |
| **Ishita Sirohi** | `include/FileStore.h`, `LayoutStore.h`, `ResultStore.h`, `bench/benchmark.cpp`, `data/` | `tests/test_framework.h`, `test_main.cpp`, `test_filestore.cpp`, `docs/TESTING.md`, `docs/BENCHMARK_RESULTS.md` |

See `docs/ARCHITECTURE.md` for design and assumptions, `docs/BENCHMARK_RESULTS.md` for measured results and their limits.
