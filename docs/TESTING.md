# Testing
<!-- MEMBER 4 (Ishita Sirohi) -->
Run `make test` (44 tests, ~680 checks, no external framework). Tests register themselves via `TEST(name)`.

| Test file | Owner | What it covers |
|---|---|---|
| `test_hashmap.cpp` | Samriddhi | overwrite, forced rehash of 5000 keys, load factor, chaining/erase, iteration |
| `test_inventory.cpp` | Samriddhi | validation, stock never negative, all-or-nothing reserve, duplicate lines |
| `test_orderqueue.cpp` | Samriddhi | heap order, priority + FIFO tie-break, aging crossover, FIFO baseline mode |
| `test_graph.cpp` | Arya | indirect shortest path, unreachable, negative/invalid edges, layouts connected |
| `test_routeplanner.cpp` | Arya | NN beats naive on a known case, dedupe, unreachable, reported distance == walked path, 2-opt <= NN on 60 random instances |
| `test_fleet.cpp` | Sundram | cheapest-fit vehicle, split loads, fleet exhausted, busy vehicles, pure assign |
| `test_manager.cpp` | Sundram | pipeline end-to-end, every rejection reason leaves state untouched, priority ordering in simulation, idle clock, deadlines, stock exhaustion, vehicle busy delay |
| `test_filestore.cpp` | Ishita | CSV round trips, corrupt/legacy lines skipped, layout loading, records output |

Known gaps (to do honestly in Phase III): no concurrency tests (the system is single-threaded), no tests on very large layouts, no fuzzing of the CSV parsers.
