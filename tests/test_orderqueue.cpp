// ===== MEMBER 2 (Samriddhi Agarwal) =====
#include "OrderQueue.h"
#include "test_framework.h"
#include <functional>

static Order mk(const std::string& id, Priority p, long tick) {
    Order o; o.id = id; o.priority = p; o.arrivalTick = tick; return o;
}

TEST(heap_pops_in_sorted_order) {
    BinaryHeap<int, std::less<int>> h;
    int vals[] = {5, 1, 9, 3, 7, 7, 2};
    for (int v : vals) h.push(v);
    int prev = 1 << 30; bool sorted = true;
    while (!h.empty()) { int x = h.pop(); if (x > prev) sorted = false; prev = x; }
    CHECK(sorted);
    bool threw = false;
    try { h.pop(); } catch (const std::out_of_range&) { threw = true; }
    CHECK(threw);
}

TEST(queue_orders_by_priority_then_fifo) {
    OrderQueue q;
    q.push(mk("n1", Priority::NORMAL, 0));
    q.push(mk("u1", Priority::URGENT, 1));
    q.push(mk("e1", Priority::EXPRESS, 2));
    q.push(mk("u2", Priority::URGENT, 1));                 // same key as u1 -> insertion order wins
    CHECK(q.pop().id == "u1"); CHECK(q.pop().id == "u2");
    CHECK(q.pop().id == "e1"); CHECK(q.pop().id == "n1");
    CHECK(q.empty());
}

TEST(queue_aging_prevents_starvation) {
    OrderQueue aged;
    aged.push(mk("old-normal", Priority::NORMAL, 0));
    aged.push(mk("fresh-urgent", Priority::URGENT, 130));  // default 50 ticks/level: > 100 ticks apart
    CHECK(aged.pop().id == "old-normal");
    OrderQueue notYet;
    notYet.push(mk("old-normal", Priority::NORMAL, 0));
    notYet.push(mk("fresh-urgent", Priority::URGENT, 60)); // only 60 ticks: urgent still wins
    CHECK(notYet.pop().id == "fresh-urgent");
    OrderQueue sharp(false, 10.0);                         // custom: 10 ticks per level -> crossover at 20
    sharp.push(mk("old-normal", Priority::NORMAL, 0));
    sharp.push(mk("fresh-urgent", Priority::URGENT, 30));
    CHECK(sharp.pop().id == "old-normal");
}

TEST(queue_fifo_mode_ignores_priority) {
    OrderQueue q(true);
    q.push(mk("a", Priority::NORMAL, 0));
    q.push(mk("b", Priority::URGENT, 1));
    q.push(mk("c", Priority::EXPRESS, 2));
    CHECK(q.pop().id == "a"); CHECK(q.pop().id == "b"); CHECK(q.pop().id == "c");
}
