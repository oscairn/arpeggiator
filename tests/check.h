// SPDX-License-Identifier: MIT
// A minimal test runner: TEST(name) { CHECK(cond); }
#pragma once
#include <cstdio>
#include <functional>
#include <vector>

struct TestCase { const char* name; void (*fn)(); };
std::vector<TestCase>& testCases();
extern int g_failures;
extern bool g_countAllocations;
extern long g_allocations;
struct TestRegistrar { TestRegistrar(const char* n, void (*f)()) { testCases().push_back({n, f}); } };
#define TEST(name) static void name(); static TestRegistrar reg_##name(#name, name); static void name()
#define CHECK(cond) do { if (!(cond)) { ++g_failures; std::printf("  %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { auto va_ = (a); auto vb_ = (b); if (!(va_ == vb_)) { ++g_failures; std::printf("  %s:%d: %s == %s failed (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, (long long)va_, (long long)vb_); } } while (0)
