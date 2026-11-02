// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <cstring>
#include <new>
#include "check.h"

// Counts allocations while g_countAllocations is set (the engine is linked statically into this binary).
// ThreadSanitizer's runtime defines operator new itself, so the count is not available there.
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define NO_ALLOCATION_COUNT 1
#endif
#endif
#if defined(__SANITIZE_THREAD__)
#define NO_ALLOCATION_COUNT 1
#endif
bool g_countAllocations = false;
long g_allocations = 0;
#ifdef NO_ALLOCATION_COUNT
extern const bool g_allocationCounting = false;
#else
extern const bool g_allocationCounting = true;
static void* counted(std::size_t n) {
    if (g_countAllocations) ++g_allocations;
    return std::malloc(n ? n : 1);
}
void* operator new(std::size_t n) {
    if (void* p = counted(n)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return operator new(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return counted(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return counted(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
#endif
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
