# Benchmark results
<!-- MEMBER 4 (Ishita Sirohi) -->
Produced by `make bench` (`bench/benchmark.cpp`) on the machine used for development. Parts 2 and 3 are deterministic
(fixed seeds); part 1 timings vary by machine - rerun on your own laptop before quoting numbers.

```
== 1. product lookup (ns per lookup, lower is better) ==
 products   linear scan       HashMap     unordered_map
      100           150             6                15
     1000          1397            46                60
    10000         15381           146               175
   100000        149361           160               217

== 2. queue policy (400 orders, 6x6 grid, mean arrival gap 28 ticks, 5 seeds averaged) ==
                    policy    wait URG    wait EXP   wait NORM    max NORM      late
           FIFO (baseline)       310.7       309.1       306.0         852      83.2
        priority, aging=10       303.1       307.4       310.0         852      82.2
        priority, aging=50       256.4       296.1       334.4         868      73.4
       priority, aging=200       110.6       249.5       418.7         967      40.2
    strict-ish, aging=5000        16.6        27.4       596.3        1786      38.4
(wait = ticks between arrival and start of picking; late = delivered after deadline, avg per run)

== 3. picking route policy (300 orders, 8x8 grid, up to 10 lines, 5 seeds averaged) ==
                policy     avg pick dist      vs naive        makespan      late
                 naive              78.9          0.0%           23844     266.8
     nearest-neighbour              52.7        -33.2%           18289       1.8
               nn+2opt              49.2        -37.6%           18246       0.0
(NN and 2-opt are heuristics: they are not guaranteed optimal.)
```

## How to read this (and what NOT to claim)
- **Hash map**: lookup stays roughly flat as the catalogue grows while a linear scan grows with N (about 1000x slower at 100k products here). At 100 products both are negligible, so the benefit only matters at scale. `std::unordered_map` is shown as a sanity reference.
- **Queue policy**: the synthetic load is *overloaded* (waits of 300 ticks), which is where priorities matter. FIFO treats everyone equally. Priority + aging cuts URGENT waits (310 -> 17 at the extreme setting) but raises NORMAL waits and the worst-case NORMAL wait (852 -> 1786). `aging=50..200` is a compromise. Late orders are also reduced, but the system is overloaded in every row, so lateness here shows the *direction* of the effect, not a real-world rate.
- **Picking**: nearest-neighbour cuts average picking distance by about a third versus typing order; 2-opt adds a few more percent. Neither is proven optimal.
- Limits: synthetic orders, one grid layout family, one picker, 5 seeds. Not evidence about real warehouses.
