# Data structures and complexity
<!-- MEMBER 2 (Samriddhi Agarwal) -->

| Structure | Where | Operation | Complexity |
|---|---|---|---|
| Hash map (chaining, djb2, load ≤ 0.75, doubles on rehash) | `HashMap.h`, `Inventory.h` | insert / find / erase | O(1) average, O(n) worst case (all keys collide) |
| Binary max-heap | `OrderQueue.h` | push / pop | O(log n) |
| Adjacency list + binary-heap Dijkstra | `Graph.h` | single-source shortest paths | O((V+E) log V) |
| Distance matrix over k stops | `RoutePlanner.h` | k+1 Dijkstra runs | O(k (V+E) log V) |
| Nearest-neighbour | `RoutePlanner.h` | visit order | O(k²) |
| 2-opt | `RoutePlanner.h` | improve order (each pass tries O(k²) reversals, each costs O(k)) | O(k³) per pass |

## Why aging
A plain priority queue can starve low-priority orders when high-priority ones keep arriving.
We use the static key `level * ticksPerLevel - arrivalTick`. For two orders the key *difference* does not depend on the current time, so the heap never needs re-keying, yet a NORMAL order that has waited more than `2 * ticksPerLevel` ticks longer than a fresh URGENT one outranks it. `ticksPerLevel` is a tunable trade-off: see `docs/BENCHMARK_RESULTS.md` (very large values give URGENT orders tiny waits but roughly double the NORMAL wait and worst-case NORMAL wait).

## Failure modes handled
- Stock can never go negative (`reserve`), multi-line orders are all-or-nothing, duplicate lines are summed before the check.
- Duplicate product ids and invalid products are rejected on insert.
- The hash map forbids copying (it owns raw nodes), preventing double-free.
