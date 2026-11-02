// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <cstring>
#include <new>
#include "check.h"

// Counts allocations while g_countAllocations is set (the engine is linked statically into this binary).
bool g_countAllocations = false;
long g_allocations = 0;
void* operator new(std::size_t n) {
    if (g_countAllocations) ++g_allocations;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
std::vector<TestCase>& testCases() { static std::vector<TestCase> v; return v; }
int g_failures = 0;
int main(int argc, char** argv) {
    int run = 0;
    for (auto& t : testCases()) {
        if (argc > 1 && !std::strstr(t.name, argv[1])) continue;
        const int before = g_failures;
        t.fn();
        ++run;
        std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", t.name);
    }
    std::printf("%d tests, %d failed checks\n", run, g_failures);
    return g_failures ? 1 : 0;
}
