// ===== MEMBER 4 (Ishita Sirohi) : test runner =====
#include "test_framework.h"

int main() {
    int before = 0;
    for (auto& t : TestRegistry::all()) {
        before = g_failures;
        t.second();
        std::cout << (g_failures == before ? "[ ok ] " : "[FAIL] ") << t.first << "\n";
    }
    std::cout << "\n" << g_checks << " checks, " << g_failures << " failed, "
              << TestRegistry::all().size() << " tests\n";
    return g_failures ? 1 : 0;
}
