# Testing
<!-- MEMBER 4 (Ishita Sirohi) -->
Run `make test` (68 tests, 793 checks, no external framework). Tests register themselves via `TEST(name)`.

| Test file | Owner | What it covers |
|---|---|---|
| `test_hashmap.cpp` | Samriddhi | overwrite, forced rehash of 5000 keys, load factor, chaining/erase, iteration |
| `test_inventory.cpp` | Samriddhi | validation, stock never negative, all-or-nothing reserve, duplicate lines |
| `test_orderqueue.cpp` | Samriddhi | heap order, priority + FIFO tie-break, aging crossover, FIFO baseline mode |
| `test_graph.cpp` | Arya | indirect shortest path, unreachable, negative/invalid edges, layouts connected |
| `test_routeplanner.cpp` | Arya | NN beats naive on a known case, dedupe, unreachable, reported distance == walked path, 2-opt <= NN on 60 random instances |
| `test_fleet.cpp` | Sundram | cheapest-fit vehicle, split loads, fleet exhausted, busy vehicles, pure assign |
| `test_manager.cpp` | Sundram | pipeline end-to-end, every rejection reason leaves state untouched, priority ordering in simulation, idle clock, deadlines, stock exhaustion, vehicle busy delay |
| `test_filestore.cpp` | Ishita | CSV round trips, corrupt/legacy lines skipped, layout loading (`data/layout.csv` must equal the built-in layout), records output |
| `test_menu.cpp` | Sundram | terminal menu: add order/product, restock, bad input asked again, policy changes |
| `test_api.cpp` | Ishita | session validation, HTTP parsing, JSON escaping, JSON API (state, add/delete order, run, compare, reset), static-file path safety, route data for the 3D view |

Known gaps (stated honestly): no concurrency stress test of the threaded server; no tests on very large layouts; no fuzzing of the CSV parsers; the browser side (3D scene, CSS) has no automated test. The page logic (`web/js/core.js`) was checked in Node against the C++ output, outside this repository, and the 3D rendering was checked by eye only.