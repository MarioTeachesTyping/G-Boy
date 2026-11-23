// ================================ //
// CPU Tests (ARM Memory Transfers) //
// ================================ //

// Halfword and signed loads/stores, SWP, and LDM/STM, including the
// ARM7TDMI quirks games and test ROMs depend on.

#include "test_harness.hpp"

using namespace gboy;
using gboy::test::boot;
using gboy::test::run_steps;

namespace
{

// Base of EWRAM, a convenient scratch area for memory tests.
constexpr u32 kRam = 0x02000000;

}

// STRH writes only the low 16 bits of a register, and LDRH zero-extends them back.
TEST(strh_ldrh_round_trip)
{
    auto gba = boot({
        0xE1C310B2, // strh r1, [r3, #2]
        0xE1D340B2, // ldrh r4, [r3, #2]
    });
    gba->cpu().set_reg(1, 0xABCD1234);
    gba->cpu().set_reg(3, kRam);
    run_steps(*gba, 2);
    CHECK_EQ(gba->bus().read16(kRam + 2), 0x1234);
    CHECK_EQ(gba->bus().read16(kRam), 0x0000);
    CHECK_EQ(gba->cpu().reg(4), 0x1234u);
}

// LDRSB and LDRSH copy the sign bit of the loaded value into the upper bits.
TEST(ldrsb_ldrsh_sign_extend)
{
    auto gba = boot({
        0xE1D300D0, // ldrsb r0, [r3]
        0xE1D310F0, // ldrsh r1, [r3]
    });
    gba->bus().write16(kRam, 0x8080);
    gba->cpu().set_reg(3, kRam);
    run_steps(*gba, 2);
    CHECK_EQ(gba->cpu().reg(0), 0xFFFFFF80u);
    CHECK_EQ(gba->cpu().reg(1), 0xFFFF8080u);
}

// Misaligned halfword loads: LDRH rotates the aligned halfword, LDRSH degrades to LDRSB.
TEST(unaligned_halfword_load_quirks)
{
    auto gba = boot({
        0xE1D300B1, // ldrh  r0, [r3, #1]
        0xE1D310F1, // ldrsh r1, [r3, #1]
    });
    gba->bus().write16(kRam, 0x8034);
    gba->cpu().set_reg(3, kRam);
    run_steps(*gba, 2);
    CHECK_EQ(gba->cpu().reg(0), 0x34000080u);
    CHECK_EQ(gba->cpu().reg(1), 0xFFFFFF80u);
}

// Post-indexed addressing accesses the base address, then always writes back base + offset.
TEST(ldrh_post_index_writes_back)
{
    auto gba = boot({
        0xE0D300B2, // ldrh r0, [r3], #2
    });
    gba->bus().write16(kRam, 0x5555);
    gba->cpu().set_reg(3, kRam);
    run_steps(*gba, 1);
    CHECK_EQ(gba->cpu().reg(0), 0x5555u);
    CHECK_EQ(gba->cpu().reg(3), kRam + 2);
}

// SWP exchanges a register with memory in one instruction; SWPB does the same with a byte.
TEST(swp_and_swpb_exchange)
{
    auto gba = boot({
        0xE1020091, // swp  r0, r1, [r2]
        0xE1424091, // swpb r4, r1, [r2]
    });
    gba->bus().write32(kRam, 0x11111111);
    gba->cpu().set_reg(1, 0x22222233);
    gba->cpu().set_reg(2, kRam);
    run_steps(*gba, 2);
    CHECK_EQ(gba->cpu().reg(0), 0x11111111u);
    CHECK_EQ(gba->cpu().reg(4), 0x33u);                 // byte read after the word swap
    CHECK_EQ(gba->bus().read32(kRam), 0x22222233u);
}

// STMIA stores ascending from the base, and LDMDB reloads the same block going downward.
TEST(stmia_then_ldmdb_round_trip)
{
    auto gba = boot({
        0xE8A0000E, // stmia r0!, {r1-r3}
        0xE9300070, // ldmdb r0!, {r4-r6}
    });
    gba->cpu().set_reg(0, kRam);
    gba->cpu().set_reg(1, 0xA);
    gba->cpu().set_reg(2, 0xB);
    gba->cpu().set_reg(3, 0xC);
    run_steps(*gba, 1);
    CHECK_EQ(gba->bus().read32(kRam + 0), 0xAu);
    CHECK_EQ(gba->bus().read32(kRam + 4), 0xBu);
    CHECK_EQ(gba->bus().read32(kRam + 8), 0xCu);
    CHECK_EQ(gba->cpu().reg(0), kRam + 12);
    run_steps(*gba, 1);
    CHECK_EQ(gba->cpu().reg(4), 0xAu);
    CHECK_EQ(gba->cpu().reg(5), 0xBu);
    CHECK_EQ(gba->cpu().reg(6), 0xCu);
    CHECK_EQ(gba->cpu().reg(0), kRam);
}

// The classic subroutine pattern: push r0 and lr, clobber r0, then pop r0 and return via pc.
TEST(push_pop_subroutine_call)
{
    auto gba = boot({
        0xE3A00001, // 0x00: mov r0, #1
        0xEB000001, // 0x04: bl func
        0xE3A02007, // 0x08: mov r2, #7
        0xEAFFFFFE, // 0x0C: b .
        0xE92D4001, // 0x10: func: stmfd sp!, {r0, lr}
        0xE3A00005, // 0x14: mov r0, #5
        0xE8BD8001, // 0x18: ldmfd sp!, {r0, pc}
    });
    run_steps(*gba, 6);
    CHECK_EQ(gba->cpu().reg(0), 1u);
    CHECK_EQ(gba->cpu().reg(2), 7u);
    CHECK_EQ(gba->cpu().reg(13), 0x03007F00u);
    CHECK_EQ(gba->cpu().pc(), 0x0800000Cu);
}

// When the base register is the first in an STM list, the original base is stored;
// when it is later in the list, the already-updated base is stored.
TEST(stm_base_in_list_quirk)
{
    auto first = boot({
        0xE8A00003, // stmia r0!, {r0, r1}
    });
    first->cpu().set_reg(0, kRam);
    run_steps(*first, 1);
    CHECK_EQ(first->bus().read32(kRam), kRam);

    auto later = boot({
        0xE8A10003, // stmia r1!, {r0, r1}
    });
    later->cpu().set_reg(1, kRam);
    run_steps(*later, 1);
    CHECK_EQ(later->bus().read32(kRam + 4), kRam + 8);
}

// When an LDM with write-back loads its own base register, the loaded value wins.
TEST(ldm_base_in_list_loaded_value_wins)
{
    auto gba = boot({
        0xE8B00003, // ldmia r0!, {r0, r1}
    });
    gba->bus().write32(kRam, 0x1234);
    gba->bus().write32(kRam + 4, 0x5678);
    gba->cpu().set_reg(0, kRam);
    run_steps(*gba, 1);
    CHECK_EQ(gba->cpu().reg(0), 0x1234u);
    CHECK_EQ(gba->cpu().reg(1), 0x5678u);
}

// An empty register list stores only r15, but moves the base by 0x40 as if all 16 were stored.
TEST(stm_empty_list_quirk)
{
    auto gba = boot({
        0xE8A00000, // stmia r0!, {}
    });
    gba->cpu().set_reg(0, kRam);
    run_steps(*gba, 1);
    CHECK_EQ(gba->bus().read32(kRam), 0x0800000Cu); // pc + 12
    CHECK_EQ(gba->cpu().reg(0), kRam + 0x40);
}

// STM with the ^ suffix in a privileged mode stores the User-mode registers, not the banked ones.
TEST(stm_user_bank_transfer)
{
    auto gba = boot({
        0xE8C02000, // stmia r0, {r13}^
    });
    gba->cpu().set_cpsr(static_cast<u32>(CpuMode::Irq));
    gba->cpu().set_reg(0, kRam);
    run_steps(*gba, 1);
    CHECK_EQ(gba->bus().read32(kRam), 0x03007F00u); // User/System SP, not IRQ's 0x03007FA0
    CHECK(gba->cpu().mode() == CpuMode::Irq);
    CHECK_EQ(gba->cpu().reg(13), 0x03007FA0u);
}

// LDM with ^ and pc in the list returns from an exception: CPSR is restored from SPSR.
TEST(ldm_with_pc_and_caret_restores_cpsr)
{
    auto gba = boot({
        0xE8D08000, // ldmia r0, {pc}^
    });
    gba->cpu().set_cpsr(static_cast<u32>(CpuMode::Irq));
    gba->cpu().set_spsr(static_cast<u32>(CpuMode::System));
    gba->cpu().set_reg(0, kRam);
    gba->bus().write32(kRam, 0x08000100);
    run_steps(*gba, 1);
    CHECK(gba->cpu().mode() == CpuMode::System);
    CHECK_EQ(gba->cpu().pc(), 0x08000100u);
}
