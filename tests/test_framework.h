// ===== MEMBER 4 (Ishita Sirohi) : tiny dependency-free test framework =====
// Usage in any tests/test_*.cpp:   TEST(name) { CHECK(cond); }
// Tests register themselves, so test files can be added/removed independently.
#pragma once
#include <iostream>
#include <utility>
#include <vector>

struct TestRegistry {
    static std::vector<std::pair<const char*, void (*)()>>& all() {
        static std::vector<std::pair<const char*, void (*)()>> v;
        return v;
    }
};
struct TestRegistrar {
    TestRegistrar(const char* name, void (*fn)()) { TestRegistry::all().push_back({name, fn}); }
};
inline int g_checks = 0, g_failures = 0;

#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failures; \
    std::cout << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } } while (0)
#define TEST(name) static void name(); static TestRegistrar reg_##name(#name, name); static void name()
