// ========================== //
// CPU Tests (ARM Exceptions) //
// ========================== //

// Software interrupts and undefined instructions, both with a (fake) BIOS to
// handle them and without one.

#include "test_harness.hpp"

using namespace gboy;
using gboy::test::make_rom;
using gboy::test::run_steps;

namespace
{

// Boots a console with a tiny fake BIOS whose SWI and undefined-instruction vectors
// immediately return with MOVS pc, lr, then jumps to the cartridge like the real BIOS would.
std::unique_ptr<Gba> boot_with_fake_bios(std::initializer_list<u32> words)
{
    std::vector<u8> bios(Bus::kBiosSize, 0);
    const u32 movs_pc_lr = 0xE1B0F00E;
    for (u32 vector : {0x04u, 0x08u})
    {
        for (int i = 0; i < 4; ++i)
        {
            bios[vector + i] = static_cast<u8>(movs_pc_lr >> (8 * i));
        }
    }

    auto gba = std::make_unique<Gba>();
    std::string error;
    gba->load_bios(bios, error);
    gba->load_rom(make_rom(words), error);
    gba->skip_bios();
    return gba;
}

}

// Without a BIOS there is nothing at the SWI vector, so the CPU halts and names the call.
TEST(swi_without_bios_halts_with_function_number)
{
    auto gba = gboy::test::boot({
        0xEF060000, // swi 0x06 (Div)
    });
    run_steps(*gba, 1);
    CHECK(gba->cpu().halted());
    CHECK(gba->cpu().error().find("SWI 0x06") != std::string::npos);
}

// With a BIOS, SWI enters Supervisor mode at 0x08 with lr = next instruction and
// SPSR = the caller's CPSR, and MOVS pc, lr returns to the caller in its original mode.
TEST(swi_enters_supervisor_and_returns)
{
    auto gba = boot_with_fake_bios({
        0xEF060000, // swi 0x06
        0xE3A01009, // mov r1, #9
    });
    run_steps(*gba, 1);
    CHECK(gba->cpu().mode() == CpuMode::Supervisor);
    CHECK_EQ(gba->cpu().pc(), 0x00000008u);
    CHECK_EQ(gba->cpu().reg(14), 0x08000004u);
    CHECK_EQ(gba->cpu().spsr() & 0x1F, static_cast<u32>(CpuMode::System));
    CHECK(gba->cpu().cpsr() & psr::I);

    run_steps(*gba, 2);
    CHECK(gba->cpu().mode() == CpuMode::System);
    CHECK_EQ(gba->cpu().reg(1), 9u);
}

// With a BIOS, an undefined instruction traps to vector 0x04 in Undefined mode.
TEST(undefined_instruction_traps_with_bios)
{
    auto gba = boot_with_fake_bios({
        0xE7F000F0, // permanently undefined encoding
    });
    run_steps(*gba, 1);
    CHECK(!gba->cpu().halted());
    CHECK(gba->cpu().mode() == CpuMode::Undefined);
    CHECK_EQ(gba->cpu().pc(), 0x00000004u);
    CHECK_EQ(gba->cpu().reg(14), 0x08000004u);
}

// Without a BIOS, an undefined instruction halts the CPU at the offending address.
TEST(undefined_instruction_halts_without_bios)
{
    auto gba = gboy::test::boot({
        0xE7F000F0, // permanently undefined encoding
    });
    run_steps(*gba, 1);
    CHECK(gba->cpu().halted());
    CHECK_EQ(gba->cpu().pc(), 0x08000000u);
}

// SUBS pc, lr, #4 is how IRQ handlers return: it restores CPSR from SPSR and jumps back.
TEST(subs_pc_lr_returns_from_irq_mode)
{
    auto gba = gboy::test::boot({
        0xE25EF004, // subs pc, lr, #4
    });
    gba->cpu().set_cpsr(static_cast<u32>(CpuMode::Irq) | psr::I);
    gba->cpu().set_spsr(static_cast<u32>(CpuMode::System));
    gba->cpu().set_reg(14, 0x08000104);
    run_steps(*gba, 1);
    CHECK(gba->cpu().mode() == CpuMode::System);
    CHECK(!(gba->cpu().cpsr() & psr::I));
    CHECK_EQ(gba->cpu().pc(), 0x08000100u);
}
