// ===== MEMBER 2 (Samriddhi Agarwal) =====
#include "HashMap.h"
#include "test_framework.h"

TEST(hashmap_insert_find_overwrite) {
    HashMap<int> m(4);
    CHECK(m.insert("a", 1));
    CHECK(!m.insert("a", 2));                 // overwrite -> false
    CHECK(*m.find("a") == 2);
    CHECK(m.find("zzz") == nullptr);
    CHECK(m.size() == 1);
}

TEST(hashmap_rehash_keeps_every_key) {
    HashMap<int> m(4);
    for (int i = 0; i < 5000; ++i) m.insert("k" + std::to_string(i), i);   // many rehashes
    CHECK(m.size() == 5000);
    bool allFound = true;
    for (int i = 0; i < 5000; ++i) { int* v = m.find("k" + std::to_string(i)); if (!v || *v != i) allFound = false; }
    CHECK(allFound);
    CHECK(static_cast<double>(m.size()) / m.bucketCount() <= 0.75 + 1e-9);   // load factor bound
}

TEST(hashmap_erase_and_collisions) {
    HashMap<int> m(1);                        // 1 bucket initially: forces chaining
    m.insert("a", 1); m.insert("b", 2); m.insert("c", 3);
    CHECK(m.erase("b")); CHECK(!m.erase("b"));
    CHECK(m.find("b") == nullptr && *m.find("a") == 1 && *m.find("c") == 3);
    CHECK(m.erase("a")); CHECK(m.erase("c"));
    CHECK(m.size() == 0);
    m.insert("x", 9);
    CHECK(*m.find("x") == 9);
}

TEST(hashmap_foreach_visits_all) {
    HashMap<int> m;
    for (int i = 0; i < 100; ++i) m.insert(std::to_string(i), i);
    long sum = 0; int n = 0;
    m.forEach([&](const std::string&, int v) { sum += v; ++n; });
    CHECK(n == 100 && sum == 4950);
}
