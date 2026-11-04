// ================= //
// Unit Test Harness //
// ================= //

// A tiny zero-dependency test framework (TEST / CHECK / CHECK_EQ) plus helpers
// for booting a console from a list of hand-assembled ARM opcodes.

#pragma once

#include <cstdio>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/gba.hpp"

namespace gboy::test
{

// One registered test: its name and the function that runs it.
struct TestCase
{
    const char* name;
    std::function<void()> fn;
};

// Returns the global list of tests, filled in by TEST() at static-initialisation time.
inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

// Returns the running count of failed CHECKs across all tests.
inline int& failures()
{
    static int count = 0;
    return count;
}

// Helper whose constructor adds a test to the registry; TEST() declares one per test.
struct Registrar
{
    Registrar(const char* name, std::function<void()> fn)
    {
        registry().push_back({name, std::move(fn)});
    }
};

// Builds a 512-byte ROM whose code starts at 0x08000000 from a list of 32-bit ARM
// opcodes. The program overwrites the header area, which is fine for tests.
inline std::vector<u8> make_rom(std::initializer_list<u32> words)
{
    std::vector<u8> rom(0x200, 0);
    std::size_t at = 0;
    for (u32 w : words)
    {
        for (int i = 0; i < 4; ++i)
        {
            rom[at++] = static_cast<u8>(w >> (8 * i));
        }
    }
    return rom;
}

// Creates a fresh console with `words` loaded as the cartridge, booted without a BIOS.
inline std::unique_ptr<Gba> boot(std::initializer_list<u32> words)
{
    auto gba = std::make_unique<Gba>();
    std::string error;
    gba->load_rom(make_rom(words), error);
    return gba;
}

// Executes `n` instructions.
inline void run_steps(Gba& gba, int n)
{
    for (int i = 0; i < n; ++i)
    {
        gba.step();
    }
}

}

#define GBOY_CONCAT_(a, b) a##b
#define GBOY_CONCAT(a, b) GBOY_CONCAT_(a, b)

// Defines and registers a test function called `name`.
#define TEST(name)                                                                                     \
    static void GBOY_CONCAT(test_fn_, name)();                                                         \
    static ::gboy::test::Registrar GBOY_CONCAT(test_reg_, name)(#name, GBOY_CONCAT(test_fn_, name));   \
    static void GBOY_CONCAT(test_fn_, name)()

// Records a failure (but keeps running) if `cond` is false.
#define CHECK(cond)                                                                                    \
    do                                                                                                 \
    {                                                                                                  \
        if (!(cond))                                                                                   \
        {                                                                                              \
            std::printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                            \
            ++::gboy::test::failures();                                                                \
        }                                                                                              \
    } while (0)

// Records a failure if `actual != expected`, printing both values in hex.
#define CHECK_EQ(actual, expected)                                                                     \
    do                                                                                                 \
    {                                                                                                  \
        const auto a_ = (actual);                                                                      \
        const auto e_ = (expected);                                                                    \
        if (!(a_ == e_))                                                                               \
        {                                                                                              \
            std::printf("    FAIL %s:%d: %s == %s (got 0x%llX, expected 0x%llX)\n", __FILE__, __LINE__, \
                        #actual, #expected, static_cast<unsigned long long>(a_),                       \
                        static_cast<unsigned long long>(e_));                                          \
            ++::gboy::test::failures();                                                                \
        }                                                                                              \
    } while (0)
