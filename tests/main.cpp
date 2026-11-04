// ================ //
// Unit Test Runner //
// ================ //

// Runs every registered test, prints a pass/fail line for each, and exits
// non-zero if anything failed so CI and `make test` can detect it.

#include "test_harness.hpp"

int main()
{
    using namespace gboy::test;

    int failed_tests = 0;
    for (const auto& t : registry())
    {
        const int before = failures();
        t.fn();
        const bool ok = failures() == before;
        if (!ok)
        {
            ++failed_tests;
        }
        std::printf("[%s] %s\n", ok ? " OK " : "FAIL", t.name);
    }

    std::printf("\n%zu tests, %d failed\n", registry().size(), failed_tests);
    return failed_tests == 0 ? 0 : 1;
}
