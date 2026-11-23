// ========================== //
// CPU Interpreter (ARM7TDMI) //
// ========================== //

// Reset, mode switching and register banking, condition-code evaluation,
// and the fetch/execute loop.

#include "core/cpu/arm7tdmi.hpp"

#include <cstdio>

#include "core/memory/bus.hpp"

namespace gboy
{

Arm7tdmi::Arm7tdmi(Bus& bus) : bus_(bus)
{
}

void Arm7tdmi::reset()
{
    regs_.fill(0);
    bank_r13_r14_ = {};
    bank_spsr_.fill(0);
    fiq_r8_r12_.fill(0);
    usr_r8_r12_.fill(0);
    cpsr_ = static_cast<u32>(CpuMode::Supervisor) | psr::I | psr::F;
    halted_ = false;
    error_.clear();
    jump(0x00000000);
}

Arm7tdmi::Bank Arm7tdmi::bank_for(CpuMode mode)
{
    switch (mode)
    {
        case CpuMode::Fiq:
            return BankFiq;
        case CpuMode::Irq:
            return BankIrq;
        case CpuMode::Supervisor:
            return BankSvc;
        case CpuMode::Abort:
            return BankAbt;
        case CpuMode::Undefined:
            return BankUnd;
        case CpuMode::User:
        case CpuMode::System:
        default:
            return BankUsr;
    }
}

void Arm7tdmi::switch_mode(CpuMode new_mode)
{
    const CpuMode old_mode = mode();
    const Bank old_bank = bank_for(old_mode);
    const Bank new_bank = bank_for(new_mode);

    if (old_bank != new_bank)
    {
        // Park the outgoing mode's SP and LR.
        bank_r13_r14_[old_bank] = {regs_[13], regs_[14]};

        // FIQ additionally has its own r8-r12, so swap those only when entering or leaving FIQ.
        const bool old_fiq = old_bank == BankFiq;
        const bool new_fiq = new_bank == BankFiq;
        if (old_fiq != new_fiq)
        {
            auto& save = old_fiq ? fiq_r8_r12_ : usr_r8_r12_;
            auto& load = new_fiq ? fiq_r8_r12_ : usr_r8_r12_;
            for (unsigned i = 0; i < 5; ++i)
            {
                save[i] = regs_[8 + i];
                regs_[8 + i] = load[i];
            }
        }

        // Bring in the incoming mode's SP and LR.
        regs_[13] = bank_r13_r14_[new_bank][0];
        regs_[14] = bank_r13_r14_[new_bank][1];
    }

    cpsr_ = (cpsr_ & ~psr::MODE_MASK) | static_cast<u32>(new_mode);
}

void Arm7tdmi::set_cpsr(u32 value)
{
    const auto new_mode = static_cast<CpuMode>(value & psr::MODE_MASK);
    if (new_mode != mode())
    {
        switch_mode(new_mode);
    }
    cpsr_ = value;
}

u32 Arm7tdmi::spsr() const
{
    const Bank b = bank_for(mode());
    return b == BankUsr ? cpsr_ : bank_spsr_[b];
}

void Arm7tdmi::set_spsr(u32 value)
{
    const Bank b = bank_for(mode());
    if (b != BankUsr)
    {
        bank_spsr_[b] = value;
    }
}

void Arm7tdmi::set_banked_sp(CpuMode target, u32 value)
{
    const Bank b = bank_for(target);
    if (b == bank_for(mode()))
    {
        // The target mode's SP is the one currently live in r13.
        regs_[13] = value;
    }
    else
    {
        bank_r13_r14_[b][0] = value;
    }
}

void Arm7tdmi::jump(u32 addr)
{
    regs_[15] = addr;
    flush_pipeline();
}

void Arm7tdmi::flush_pipeline()
{
    if (thumb())
    {
        regs_[15] = (regs_[15] & ~1u) + 4;
    }
    else
    {
        regs_[15] = (regs_[15] & ~3u) + 8;
    }
    flushed_ = true;
}

bool Arm7tdmi::condition_passed(u32 cond) const
{
    const bool n = cpsr_ & psr::N;
    const bool z = cpsr_ & psr::Z;
    const bool c = cpsr_ & psr::C;
    const bool v = cpsr_ & psr::V;

    switch (cond)
    {
        case 0x0: return z;            // EQ: equal
        case 0x1: return !z;           // NE: not equal
        case 0x2: return c;            // CS/HS: unsigned >=
        case 0x3: return !c;           // CC/LO: unsigned <
        case 0x4: return n;            // MI: negative
        case 0x5: return !n;           // PL: positive or zero
        case 0x6: return v;            // VS: overflow
        case 0x7: return !v;           // VC: no overflow
        case 0x8: return c && !z;      // HI: unsigned >
        case 0x9: return !c || z;      // LS: unsigned <=
        case 0xA: return n == v;       // GE: signed >=
        case 0xB: return n != v;       // LT: signed <
        case 0xC: return !z && n == v; // GT: signed >
        case 0xD: return z || n != v;  // LE: signed <=
        case 0xE: return true;         // AL: always
        default:  return false;        // NV: never (reserved on ARMv4)
    }
}

void Arm7tdmi::set_nz(u32 result)
{
    cpsr_ &= ~(psr::N | psr::Z);
    if (result & 0x80000000u)
    {
        cpsr_ |= psr::N;
    }
    if (result == 0)
    {
        cpsr_ |= psr::Z;
    }
}

void Arm7tdmi::fail(const std::string& message)
{
    char where[32];
    std::snprintf(where, sizeof(where), " at 0x%08X", pc());
    error_ = message + where;
    halted_ = true;
}

void Arm7tdmi::raise_exception(CpuMode new_mode, u32 vector, u32 return_addr)
{
    const u32 old_cpsr = cpsr_;
    switch_mode(new_mode);
    set_spsr(old_cpsr);
    regs_[14] = return_addr;

    // Exception handlers always start in ARM state with IRQs masked; FIQ entry also masks FIQs.
    cpsr_ &= ~psr::T;
    cpsr_ |= psr::I;
    if (new_mode == CpuMode::Fiq)
    {
        cpsr_ |= psr::F;
    }
    jump(vector);
}

int Arm7tdmi::undefined_instruction()
{
    if (!bios_loaded_)
    {
        fail("Undefined instruction (no BIOS loaded to handle the exception)");
        return 1;
    }

    // The handler returns with MOVS pc, lr, so lr must hold the address of the next instruction.
    raise_exception(CpuMode::Undefined, 0x00000004, pc() + (thumb() ? 2 : 4));
    return 3;
}

int Arm7tdmi::step()
{
    if (halted_)
    {
        return 1;
    }

    if (thumb())
    {
        fail("Thumb state not implemented yet (M2)");
        return 1;
    }

    // r15 is 8 bytes ahead of the instruction being executed, so fetch from r15 - 8.
    const u32 instr = bus_.read32(regs_[15] - 8);
    flushed_ = false;
    const int cycles = execute_arm(instr);

    // Only advance to the next instruction if this one didn't branch (or halt).
    if (!flushed_ && !halted_)
    {
        regs_[15] += 4;
    }
    return cycles;
}

}
