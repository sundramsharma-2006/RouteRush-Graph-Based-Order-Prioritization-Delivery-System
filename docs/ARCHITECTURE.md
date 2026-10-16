# Architecture (backend)
<!-- MEMBER 3 (Arya Kumari) -->

```
 data/*.csv ──► FileStore / LayoutStore ──► Inventory (HashMap)      Graph (adjacency list)
 (products, orders, layout)                      │                          │
                                                 ▼                          ▼
 orders ──► OrderQueue (heap + aging) ──► WarehouseManager ──► RoutePlanner (Dijkstra + NN + 2-opt)
                                                 │
                                                 ├──► Fleet (vehicle choice, split loads)
                                                 ▼
                                      DeliveryRecord / Metrics ──► ResultStore (out_*.csv)
```

Pipeline for one order (all checks happen **before** anything is changed, so a rejected order leaves stock and fleet untouched):

1. validate (lines, quantities, destination) → 2. known products + weight → 3. plan picking route → 4. plan vehicles (pure) → 5. reserve stock (all-or-nothing) → 6. commit vehicles, record result.

| Module | Responsibility | Owner |
|---|---|---|
| `Types.h` | plain data: Product, Order, DeliveryRecord | Sundram |
| `Vehicle.h`, `Fleet.h` | vehicle hierarchy (polymorphism), vehicle selection | Sundram |
| `WarehouseManager.h` | pipeline + event simulation + metrics | Sundram |
| `HashMap.h`, `Inventory.h` | O(1) average product lookup, safe stock updates | Samriddhi |
| `OrderQueue.h` | binary heap, priority + aging, FIFO baseline mode | Samriddhi |
| `Graph.h`, `WarehouseLayout.h`, `RoutePlanner.h` | graph, Dijkstra, picking-route heuristics | Arya |
| `FileStore.h`, `LayoutStore.h`, `ResultStore.h` | CSV persistence with validation | Ishita |

## Assumptions and limits (be ready to defend these)
- Time is a logical tick clock; 1 distance unit = 1 tick. There is one picker (no parallel picking).
- Distances are static; there is no congestion or travel-time variation. Data is simulated.
- Picking-route heuristics (nearest-neighbour, 2-opt) are **not optimal**; they are compared against the naive order, not against an exact solver.
- Fleet is finite; a vehicle is busy for a round trip. Orders heavier than one vehicle are split across vehicles; if the fleet cannot carry the load the order is rejected.
