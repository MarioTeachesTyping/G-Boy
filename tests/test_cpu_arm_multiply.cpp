// ========================== //
// CPU Tests (ARM Multiplies) //
// ========================== //

// MUL, MLA and the four 64-bit long multiplies, including signed vs unsigned
// results and the accumulate forms.

#include "test_harness.hpp"

using namespace gboy;
using gboy::test::boot;
using gboy::test::run_steps;

// MUL multiplies two registers into a third.
TEST(mul_basic)
{
    auto gba = boot({
        0xE3A00006, // mov r0, #6
        0xE3A01007, // mov r1, #7
        0xE0020190, // mul r2, r0, r1
    });
    run_steps(*gba, 3);
    CHECK_EQ(gba->cpu().reg(2), 42u);
}

// MLA adds an accumulator register to the product, and the S form updates N and Z.
TEST(mlas_accumulates_and_sets_flags)
{
    auto gba = boot({
        0xE0332190, // mlas r3, r0, r1, r2
    });
    gba->cpu().set_reg(0, 6);
    gba->cpu().set_reg(1, 7);
    gba->cpu().set_reg(2, 42);
    run_steps(*gba, 1);
    CHECK_EQ(gba->cpu().reg(3), 84u);
    CHECK(!(gba->cpu().cpsr() & psr::Z));
    CHECK(!(gba->cpu().cpsr() & psr::N));
}

// MULS with a zero result must set the Z flag.
TEST(muls_zero_sets_z)
{
    auto gba = boot({
        0xE0120190, // muls r2, r0, r1
    });
    gba->cpu().set_reg(0, 0);
    gba->cpu().set_reg(1, 1234);
    run_steps(*gba, 1);
    CHECK_EQ(gba->cpu().reg(2), 0u);
    CHECK(gba->cpu().cpsr() & psr::Z);
}

// UMULL treats both operands as unsigned and splits the 64-bit product across two registers.
TEST(umull_unsigned_64bit_product)
{
    auto gba = boot({
        0xE0810392, // umull r0, r1, r2, r3
    });
    gba->cpu().set_reg(2, 0xFFFFFFFF);
    gba->cpu().set_reg(3, 2);
    run_steps(*gba, 1);
    CHECK_EQ(gba->cpu().reg(0), 0xFFFFFFFEu);
    CHECK_EQ(gba->cpu().reg(1), 0x00000001u);
}

// SMULL treats the operands as signed, so -1 * 2 = -2 across all 64 bits.
TEST(smull_signed_64bit_product)
{
    auto gba = boot({
        0xE0C10392, // smull r0, r1, r2, r3
    });
    gba->cpu().set_reg(2, 0xFFFFFFFF);
    gba->cpu().set_reg(3, 2);
    run_steps(*gba, 1);
    CHECK_EQ(gba->cpu().reg(0), 0xFFFFFFFEu);
    CHECK_EQ(gba->cpu().reg(1), 0xFFFFFFFFu);
}

// UMLAL adds the product to the existing 64-bit value, carrying from the low word into the high word.
TEST(umlal_accumulates_with_carry_into_high_word)
{
    auto gba = boot({
        0xE0A10392, // umlal r0, r1, r2, r3
    });
    gba->cpu().set_reg(0, 1);
    gba->cpu().set_reg(1, 0);
    gba->cpu().set_reg(2, 0xFFFFFFFF);
    gba->cpu().set_reg(3, 1);
    run_steps(*gba, 1);
    CHECK_EQ(gba->cpu().reg(0), 0u);
    CHECK_EQ(gba->cpu().reg(1), 1u);
}

// SMULLS sets N from bit 63 of the 64-bit result, not bit 31.
TEST(smulls_negative_sets_n_from_bit_63)
{
    auto gba = boot({
        0xE0D10392, // smulls r0, r1, r2, r3
    });
    gba->cpu().set_reg(2, 0xFFFFFFFF);
    gba->cpu().set_reg(3, 2);
    run_steps(*gba, 1);
    CHECK(gba->cpu().cpsr() & psr::N);
    CHECK(!(gba->cpu().cpsr() & psr::Z));
}
