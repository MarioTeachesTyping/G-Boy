// =============================== //
// CPU Tests (ARM Instruction Set) //
// =============================== //

// Each test boots a console from hand-assembled ARM opcodes and checks the
// resulting registers, flags and memory. The comment beside each opcode is
// the assembly it encodes.

#include "test_harness.hpp"

using namespace gboy;
using gboy::test::boot;
using gboy::test::run_steps;

// Booting without a BIOS should leave the CPU where the real BIOS would hand off to the game.
TEST(boots_without_bios_into_system_mode)
{
    auto gba = boot({});
    CHECK_EQ(gba->cpu().pc(), 0x08000000u);
    CHECK(gba->cpu().mode() == CpuMode::System);
    CHECK_EQ(gba->cpu().reg(13), 0x03007F00u);
}

// The simplest data-processing ops: load an immediate, then add to it.
TEST(mov_and_add_immediate)
{
    auto gba = boot({
        0xE3A00005, // mov r0, #5
        0xE2801003, // add r1, r0, #3
    });
    run_steps(*gba, 2);
    CHECK_EQ(gba->cpu().reg(0), 5u);
    CHECK_EQ(gba->cpu().reg(1), 8u);
    CHECK_EQ(gba->cpu().pc(), 0x08000008u);
}

// SUBS producing zero must set Z and C (no borrow), and conditional execution must follow Z.
TEST(subs_sets_zero_and_carry)
{
    auto gba = boot({
        0xE3A00005, // mov r0, #5
        0xE2502005, // subs r2, r0, #5
        0x03A03001, // moveq r3, #1
        0x13A04001, // movne r4, #1
    });
    run_steps(*gba, 4);
    const u32 cpsr = gba->cpu().cpsr();
    CHECK(cpsr & psr::Z);
    CHECK(cpsr & psr::C);
    CHECK(!(cpsr & psr::N));
    CHECK_EQ(gba->cpu().reg(3), 1u);
    CHECK_EQ(gba->cpu().reg(4), 0u);
}

// 0x80000000 + 0x80000000 wraps to 0 with both unsigned carry and signed overflow.
TEST(adds_signed_overflow)
{
    auto gba = boot({
        0xE3A00102, // mov r0, #0x80000000
        0xE0901000, // adds r1, r0, r0
    });
    run_steps(*gba, 2);
    const u32 cpsr = gba->cpu().cpsr();
    CHECK_EQ(gba->cpu().reg(1), 0u);
    CHECK(cpsr & psr::Z);
    CHECK(cpsr & psr::C);
    CHECK(cpsr & psr::V);
}

// Immediate barrel-shifter operands, including the carry-out a logical MOVS puts in C.
TEST(barrel_shifter_immediate_shifts)
{
    auto gba = boot({
        0xE3A00001, // mov r0, #1
        0xE1A01200, // mov r1, r0, lsl #4
        0xE1B020A0, // movs r2, r0, lsr #1
    });
    run_steps(*gba, 3);
    CHECK_EQ(gba->cpu().reg(1), 16u);
    CHECK_EQ(gba->cpu().reg(2), 0u);
    CHECK(gba->cpu().cpsr() & psr::C);
    CHECK(gba->cpu().cpsr() & psr::Z);
}

// A forward branch must skip the instructions in between.
TEST(branch_skips_instructions)
{
    auto gba = boot({
        0xEA000001, // b +0x0C
        0xE3A00001, // mov r0, #1   (skipped)
        0xE3A00002, // mov r0, #2   (skipped)
        0xE3A01003, // mov r1, #3
    });
    run_steps(*gba, 2);
    CHECK_EQ(gba->cpu().reg(0), 0u);
    CHECK_EQ(gba->cpu().reg(1), 3u);
}

// BL must store the address of the instruction after itself in the link register.
TEST(branch_with_link_sets_lr)
{
    auto gba = boot({
        0xEB000000, // bl +0x08
        0xE3A00001, // mov r0, #1   (skipped)
        0xE3A01002, // mov r1, #2
    });
    run_steps(*gba, 1);
    CHECK_EQ(gba->cpu().reg(14), 0x08000004u);
    CHECK_EQ(gba->cpu().pc(), 0x08000008u);
}

// A word stored to EWRAM must read back, and also appear at the 256 KB mirror.
TEST(store_then_load_ewram)
{
    auto gba = boot({
        0xE3A03402, // mov r3, #0x02000000
        0xE3A01042, // mov r1, #0x42
        0xE5831000, // str r1, [r3]
        0xE5934000, // ldr r4, [r3]
    });
    run_steps(*gba, 4);
    CHECK_EQ(gba->cpu().reg(4), 0x42u);
    CHECK_EQ(gba->bus().read32(0x02000000), 0x42u);
    CHECK_EQ(gba->bus().read32(0x02040000), 0x42u);
}

// A PC-relative load must see r15 as the instruction address + 8 (the pipeline offset).
TEST(pc_relative_load_reads_pc_plus_8)
{
    auto gba = boot({
        0xE59F0000, // ldr r0, [pc, #0]   -> address 0x08000008
        0x00000000,
        0xDEADBEEF,
    });
    run_steps(*gba, 1);
    CHECK_EQ(gba->cpu().reg(0), 0xDEADBEEFu);
}

// An LDR from a misaligned address must return the aligned word rotated right.
TEST(unaligned_ldr_rotates)
{
    auto gba = boot({
        0xE3A03402, // mov r3, #0x02000000
        0xE5931001, // ldr r1, [r3, #1]
    });
    gba->bus().write32(0x02000000, 0x11223344);
    run_steps(*gba, 2);
    CHECK_EQ(gba->cpu().reg(1), 0x44112233u);
}

// Switching mode with MSR must swap in that mode's stack pointer, and MRS must read it back.
TEST(msr_switches_mode_and_banks_sp)
{
    auto gba = boot({
        0xE321F012, // msr cpsr_c, #0x12   (IRQ mode)
        0xE10F0000, // mrs r0, cpsr
    });
    run_steps(*gba, 2);
    CHECK(gba->cpu().mode() == CpuMode::Irq);
    CHECK_EQ(gba->cpu().reg(13), 0x03007FA0u);
    CHECK_EQ(gba->cpu().reg(0) & 0x1F, 0x12u);
}

// FIQ mode has private r8-r12; leaving FIQ must restore the originals untouched.
TEST(fiq_banks_r8_to_r12)
{
    auto gba = boot({});
    Arm7tdmi& cpu = gba->cpu();
    cpu.set_reg(8, 0x1111);
    cpu.set_cpsr(static_cast<u32>(CpuMode::Fiq));
    CHECK_EQ(cpu.reg(8), 0u);
    cpu.set_reg(8, 0x2222);
    cpu.set_cpsr(static_cast<u32>(CpuMode::System));
    CHECK_EQ(cpu.reg(8), 0x1111u);
    CHECK_EQ(cpu.reg(13), 0x03007F00u);
}

// BX to an odd address must enter Thumb state. Thumb isn't implemented until M2, so the
// next step must halt cleanly with an error instead of executing garbage.
TEST(bx_enters_thumb_then_halts_until_m2)
{
    auto gba = boot({
        0xE12FFF10, // bx r0
    });
    gba->cpu().set_reg(0, 0x08000009);
    run_steps(*gba, 3);
    CHECK(gba->cpu().thumb());
    CHECK_EQ(gba->cpu().pc(), 0x08000008u);
    CHECK(gba->cpu().halted());
    CHECK(!gba->cpu().error().empty());
}

// A compare op with S set and Rd = 15 (e.g. the odd encoding "cmp pc, pc, r0") restores
// CPSR from SPSR, switching modes, but must not branch. Found by jsmolka arm.gba test 234/235.
TEST(compare_with_rd_pc_restores_cpsr_without_branching)
{
    auto gba = boot({
        0xE15FF000, // cmp pc, pc, r0   (Rd = 15)
        0xE3A00001, // mov r0, #1
    });
    gba->cpu().set_cpsr(static_cast<u32>(CpuMode::Fiq));
    gba->cpu().set_spsr(static_cast<u32>(CpuMode::System));
    run_steps(*gba, 2);
    CHECK(gba->cpu().mode() == CpuMode::System);
    CHECK_EQ(gba->cpu().reg(0), 1u);
    CHECK_EQ(gba->cpu().pc(), 0x08000008u);
}
