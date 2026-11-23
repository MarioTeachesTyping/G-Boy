// ========================== //
// CPU Interpreter (ARM7TDMI) //
// ========================== //

// The GBA's processor: register file, the seven operating modes with their
// banked registers, condition codes, and the fetch/execute loop. The actual
// instruction handlers live in arm_isa.cpp (and thumb_isa.cpp from M2).

#pragma once

#include <array>
#include <string>

#include "core/common.hpp"

namespace gboy
{

class Bus;

// The processor modes. The value of each enumerator is its encoding in the low
// five bits of CPSR. Each privileged mode has its own stack pointer and link register.
enum class CpuMode : u32
{
    User = 0x10,       // Normal unprivileged execution.
    Fiq = 0x11,        // Fast interrupt (unused by GBA hardware, but still emulated).
    Irq = 0x12,        // Normal interrupt handler.
    Supervisor = 0x13, // Entered on reset and by SWI (BIOS calls).
    Abort = 0x17,      // Memory abort (never raised on the GBA).
    Undefined = 0x1B,  // Entered on an undefined instruction.
    System = 0x1F,     // Privileged mode sharing User's registers; games mostly run here.
};

// Bit masks for the fields of the program status registers (CPSR / SPSR).
namespace psr
{
inline constexpr u32 N = 1u << 31;        // Negative: result had bit 31 set.
inline constexpr u32 Z = 1u << 30;        // Zero: result was zero.
inline constexpr u32 C = 1u << 29;        // Carry: unsigned overflow / no borrow / shifter carry-out.
inline constexpr u32 V = 1u << 28;        // Overflow: signed overflow.
inline constexpr u32 I = 1u << 7;         // When set, IRQs are disabled.
inline constexpr u32 F = 1u << 6;         // When set, FIQs are disabled.
inline constexpr u32 T = 1u << 5;         // When set, the CPU executes 16-bit Thumb code.
inline constexpr u32 MODE_MASK = 0x1F;    // The CpuMode bits.
}

// ARM7TDMI interpreter.
//
// Pipeline model: the real CPU fetches two instructions ahead, so while an
// instruction executes, r15 reads as (its own address + 8) in ARM state or
// (+ 4) in Thumb state. We keep r15 in that "+8" form at all times, and anything
// that writes r15 calls flush_pipeline() to re-establish it.
class Arm7tdmi
{
public:
    // Creates a CPU that performs all memory accesses through `bus`.
    explicit Arm7tdmi(Bus& bus);

    // Performs a hardware reset: Supervisor mode, IRQ/FIQ disabled, execution at 0x00000000.
    void reset();

    // Fetches and executes one instruction. Returns the number of cycles it took.
    int step();

    // Reads general-purpose register `index` (0-15) as seen by the current mode.
    u32 reg(unsigned index) const
    {
        return regs_[index];
    }

    // Writes general-purpose register `index` in the current mode. Writing r15 this way
    // does not refill the pipeline; use jump() for that.
    void set_reg(unsigned index, u32 value)
    {
        regs_[index] = value;
    }

    // Returns the Current Program Status Register (flags, mode, interrupt masks, Thumb bit).
    u32 cpsr() const
    {
        return cpsr_;
    }

    // Replaces CPSR. If the mode bits change, the banked registers are swapped accordingly.
    void set_cpsr(u32 value);

    // Returns the Saved Program Status Register of the current mode. User and System mode
    // have no SPSR, so in those modes this returns CPSR instead.
    u32 spsr() const;

    // Writes the current mode's SPSR. Ignored in User and System mode.
    void set_spsr(u32 value);

    // Returns the current processor mode decoded from CPSR.
    CpuMode mode() const
    {
        return static_cast<CpuMode>(cpsr_ & psr::MODE_MASK);
    }

    // Returns whether the CPU is in Thumb state.
    bool thumb() const
    {
        return cpsr_ & psr::T;
    }

    // Returns the address of the instruction that will execute next, undoing the
    // pipeline offset baked into r15.
    u32 pc() const
    {
        return regs_[15] - (thumb() ? 4 : 8);
    }

    // Moves execution to `addr` and refills the pipeline, exactly like a taken branch.
    void jump(u32 addr);

    // Sets the stack pointer belonging to `mode` without switching into that mode.
    // Used to set up the per-mode stacks the BIOS would normally initialise.
    void set_banked_sp(CpuMode mode, u32 value);

    // Tells the CPU whether a real BIOS is mapped at 0x00000000. Without one, the SWI and
    // undefined-instruction vectors point at empty memory, so instead of jumping there the
    // CPU halts with an explanatory error (until high-level BIOS emulation lands in M3).
    void set_bios_loaded(bool loaded)
    {
        bios_loaded_ = loaded;
    }

    // Returns whether the CPU has stopped because of an error (e.g. an unimplemented opcode).
    bool halted() const
    {
        return halted_;
    }

    // Returns a human-readable description of why the CPU halted, or "" if it hasn't.
    const std::string& error() const
    {
        return error_;
    }

private:
    // Slots in the banked-register storage. User and System share BankUsr.
    enum Bank : unsigned
    {
        BankUsr,
        BankFiq,
        BankIrq,
        BankSvc,
        BankAbt,
        BankUnd,
        BankCount
    };

    // Returns which register bank a given mode uses.
    static Bank bank_for(CpuMode mode);

    // Swaps the banked registers (r8-r14 as needed) out for the ones belonging to
    // `new_mode` and updates the mode bits in CPSR.
    void switch_mode(CpuMode new_mode);

    // Re-applies the pipeline offset after r15 has been written (aligning it and adding
    // 8 or 4) and marks the current instruction as having changed control flow.
    void flush_pipeline();

    // Evaluates a 4-bit ARM condition code (EQ, NE, ... AL) against the current flags.
    bool condition_passed(u32 cond) const;

    // Updates the N and Z flags from `result`, leaving C and V untouched.
    void set_nz(u32 result);

    // Halts the CPU and records `message` (plus the faulting address) for the frontend.
    void fail(const std::string& message);

    // Takes a CPU exception: saves CPSR into the new mode's SPSR, switches to `new_mode`
    // in ARM state with IRQs disabled, stores `return_addr` in that mode's r14, and jumps
    // to the exception vector at `vector`.
    void raise_exception(CpuMode new_mode, u32 vector, u32 return_addr);

    // Enters the undefined-instruction handler (vector 0x04), or halts if there is no BIOS.
    int undefined_instruction();

    // Decodes a 32-bit ARM opcode and dispatches to the matching handler below.
    int execute_arm(u32 instr);

    // B / BL: PC-relative branch, optionally saving the return address in r14.
    int arm_branch(u32 instr);

    // BX: branch to an address in a register, switching to Thumb if bit 0 is set.
    int arm_branch_exchange(u32 instr);

    // The 16 ALU operations (AND, ADD, CMP, MOV, ...), with optional flag updates.
    int arm_data_processing(u32 instr);

    // MRS: copy CPSR or SPSR into a register.
    int arm_mrs(u32 instr);

    // MSR: write some or all fields of CPSR or SPSR from a register or immediate.
    int arm_msr(u32 instr);

    // LDR / STR / LDRB / STRB: single word or byte memory transfer.
    int arm_single_transfer(u32 instr);

    // MUL / MLA: 32-bit multiply, optionally adding an accumulator register.
    int arm_multiply(u32 instr);

    // UMULL / UMLAL / SMULL / SMLAL: 32x32 -> 64-bit multiply, optionally accumulating.
    int arm_multiply_long(u32 instr);

    // LDRH / STRH / LDRSB / LDRSH: halfword and sign-extending memory transfers.
    int arm_halfword_transfer(u32 instr);

    // SWP / SWPB: atomically swap a register with a word or byte in memory.
    int arm_swap(u32 instr);

    // LDM / STM: load or store any subset of r0-r15 to consecutive memory words.
    int arm_block_transfer(u32 instr);

    // SWI: software interrupt, used by games to call BIOS functions.
    int arm_software_interrupt(u32 instr);

    // Computes a data-processing instruction's second operand by running it through the
    // barrel shifter. Also produces the shifter's carry-out, which logical ops put in C.
    u32 shifter_operand(u32 instr, bool& carry_out);

    // The memory bus every fetch, load and store goes through.
    Bus& bus_;

    // r0-r15 as visible in the current mode. r13 = SP, r14 = LR, r15 = PC (+8 / +4).
    std::array<u32, 16> regs_{};

    // Current Program Status Register.
    u32 cpsr_ = 0;

    // Saved r13/r14 for every bank that is not currently active.
    std::array<std::array<u32, 2>, BankCount> bank_r13_r14_{};

    // Each privileged mode's SPSR. The BankUsr entry is unused.
    std::array<u32, BankCount> bank_spsr_{};

    // FIQ mode's private copies of r8-r12.
    std::array<u32, 5> fiq_r8_r12_{};

    // Every other mode's r8-r12, parked here while FIQ mode is active.
    std::array<u32, 5> usr_r8_r12_{};

    // Set by flush_pipeline() so step() knows not to advance r15 after the instruction.
    bool flushed_ = false;

    // Whether a BIOS image is mapped, i.e. whether exception vectors lead anywhere useful.
    bool bios_loaded_ = false;

    // True once the CPU has hit something it cannot execute; step() then does nothing.
    bool halted_ = false;

    // Description of the error that halted the CPU.
    std::string error_;
};

}
