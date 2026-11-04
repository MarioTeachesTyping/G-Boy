// ===================================== //
// CPU Interpreter (ARM Instruction Set) //
// ===================================== //

// Decoding and execution of 32-bit ARM instructions.
//
// Implemented (M0): B, BL, BX, all 16 data-processing ops with the full
// barrel shifter, MRS, MSR, LDR/STR/LDRB/STRB.
// Planned (M1):     MUL/MLA, long multiplies, halfword & signed transfers,
//                   LDM/STM, SWP, SWI and the undefined-instruction trap.

#include "core/cpu/arm7tdmi.hpp"
#include "core/memory/bus.hpp"

namespace gboy
{

namespace
{

// The four barrel-shifter operations, as encoded in bits 6-5 of an opcode.
enum ShiftType : u32
{
    LSL = 0, // Logical shift left.
    LSR = 1, // Logical shift right (fills with zeros).
    ASR = 2, // Arithmetic shift right (fills with the sign bit).
    ROR = 3, // Rotate right.
};

// Shifts `value` by an amount taken from a register (0-255). An amount of 0 leaves
// both the value and the carry untouched; amounts of 32 or more have their own rules.
u32 shift_by_amount(u32 type, u32 value, u32 amount, bool carry_in, bool& carry_out)
{
    carry_out = carry_in;
    if (amount == 0)
    {
        return value;
    }

    switch (type)
    {
        case LSL:
            if (amount < 32)
            {
                carry_out = bit(value, 32 - amount);
                return value << amount;
            }
            carry_out = amount == 32 ? bit(value, 0) : false;
            return 0;

        case LSR:
            if (amount < 32)
            {
                carry_out = bit(value, amount - 1);
                return value >> amount;
            }
            carry_out = amount == 32 ? bit(value, 31) : false;
            return 0;

        case ASR:
            if (amount < 32)
            {
                carry_out = bit(value, amount - 1);
                return static_cast<u32>(static_cast<s32>(value) >> amount);
            }
            carry_out = bit(value, 31);
            return carry_out ? 0xFFFFFFFFu : 0;

        case ROR:
        default:
        {
            const u32 r = amount & 31;
            if (r == 0)
            {
                // A rotate by a multiple of 32 leaves the value alone but still sets C from bit 31.
                carry_out = bit(value, 31);
                return value;
            }
            carry_out = bit(value, r - 1);
            return ror32(value, r);
        }
    }
}

// Shifts `value` by a 5-bit amount encoded directly in the opcode. Because a shift of
// 0 would be pointless for most types, the hardware reuses those encodings:
// LSR #0 means LSR #32, ASR #0 means ASR #32, and ROR #0 means RRX (rotate right by
// one bit through the carry flag).
u32 shift_by_immediate(u32 type, u32 value, u32 amount, bool carry_in, bool& carry_out)
{
    if (amount == 0)
    {
        switch (type)
        {
            case LSL:
                carry_out = carry_in;
                return value;
            case LSR:
            case ASR:
                return shift_by_amount(type, value, 32, carry_in, carry_out);
            case ROR:
            default:
                carry_out = bit(value, 0);
                return (static_cast<u32>(carry_in) << 31) | (value >> 1);
        }
    }
    return shift_by_amount(type, value, amount, carry_in, carry_out);
}

// The outcome of an arithmetic ALU operation: the 32-bit result plus the carry
// and signed-overflow flags it produced.
struct AluResult
{
    u32 value;
    bool carry;
    bool overflow;
};

// Computes a + b + carry with ARM flag semantics. Every subtraction is expressed as
// a + ~b + 1 (or + C for SBC/RSC), which naturally yields ARM's "carry = NOT borrow".
AluResult add_with_carry(u32 a, u32 b, bool carry)
{
    const u64 wide = static_cast<u64>(a) + b + (carry ? 1 : 0);
    const u32 result = static_cast<u32>(wide);
    // Signed overflow happens when both inputs share a sign that differs from the result's.
    const bool overflow = ((~(a ^ b) & (a ^ result)) >> 31) != 0;
    return {result, (wide >> 32) != 0, overflow};
}

}

int Arm7tdmi::execute_arm(u32 instr)
{
    if (!condition_passed(instr >> 28))
    {
        return 1;
    }

    // The checks are ordered from most to least specific, because several instruction
    // classes share bit patterns with the broad data-processing encoding.
    if ((instr & 0x0FFFFFF0) == 0x012FFF10)
    {
        return arm_branch_exchange(instr);
    }
    if ((instr & 0x0E000000) == 0x0A000000)
    {
        return arm_branch(instr);
    }
    if ((instr & 0x0FBF0FFF) == 0x010F0000)
    {
        return arm_mrs(instr);
    }
    if ((instr & 0x0FB0FFF0) == 0x0120F000 || (instr & 0x0FB0F000) == 0x0320F000)
    {
        return arm_msr(instr);
    }
    if ((instr & 0x0FC000F0) == 0x00000090)
    {
        fail("ARM MUL/MLA not implemented (M1)");
        return 1;
    }
    if ((instr & 0x0F8000F0) == 0x00800090)
    {
        fail("ARM long multiply not implemented (M1)");
        return 1;
    }
    if ((instr & 0x0FB00FF0) == 0x01000090)
    {
        fail("ARM SWP not implemented (M1)");
        return 1;
    }
    if ((instr & 0x0E000090) == 0x00000090)
    {
        fail("ARM halfword transfer not implemented (M1)");
        return 1;
    }
    if ((instr & 0x0C000000) == 0x00000000)
    {
        return arm_data_processing(instr);
    }
    if ((instr & 0x0E000010) == 0x06000010)
    {
        fail("ARM undefined instruction");
        return 1;
    }
    if ((instr & 0x0C000000) == 0x04000000)
    {
        return arm_single_transfer(instr);
    }
    if ((instr & 0x0E000000) == 0x08000000)
    {
        fail("ARM LDM/STM not implemented (M1)");
        return 1;
    }
    if ((instr & 0x0F000000) == 0x0F000000)
    {
        fail("ARM SWI not implemented (M1)");
        return 1;
    }

    fail("ARM coprocessor instruction (unsupported on GBA)");
    return 1;
}

int Arm7tdmi::arm_branch(u32 instr)
{
    // The 24-bit offset counts words, so it is sign-extended and multiplied by 4.
    const s32 offset = sign_extend(instr & 0x00FFFFFF, 24) * 4;
    if (bit(instr, 24))
    {
        // BL: r15 is 8 ahead, so r15 - 4 is the instruction right after this one.
        regs_[14] = regs_[15] - 4;
    }
    regs_[15] += static_cast<u32>(offset);
    flush_pipeline();
    return 3;
}

int Arm7tdmi::arm_branch_exchange(u32 instr)
{
    // Bit 0 of the target selects the instruction set: 1 = Thumb, 0 = ARM.
    const u32 target = regs_[instr & 0xF];
    if (target & 1)
    {
        cpsr_ |= psr::T;
    }
    else
    {
        cpsr_ &= ~psr::T;
    }
    regs_[15] = target;
    flush_pipeline();
    return 3;
}

u32 Arm7tdmi::shifter_operand(u32 instr, bool& carry_out)
{
    const bool carry_in = cpsr_ & psr::C;

    if (bit(instr, 25))
    {
        // Immediate operand: an 8-bit value rotated right by twice the 4-bit rotate field.
        const u32 rotate = bits(instr, 11, 8) * 2;
        const u32 value = ror32(instr & 0xFF, rotate);
        carry_out = rotate == 0 ? carry_in : bit(value, 31);
        return value;
    }

    const u32 rm_index = instr & 0xF;
    const u32 type = bits(instr, 6, 5);

    if (bit(instr, 4))
    {
        // Register-specified shift amount. The extra internal cycle this costs means the
        // pipeline has moved on further, so r15 reads as instruction + 12 here.
        const u32 rm = regs_[rm_index] + (rm_index == 15 ? 4 : 0);
        const u32 amount = regs_[bits(instr, 11, 8)] & 0xFF;
        return shift_by_amount(type, rm, amount, carry_in, carry_out);
    }

    return shift_by_immediate(type, regs_[rm_index], bits(instr, 11, 7), carry_in, carry_out);
}

int Arm7tdmi::arm_data_processing(u32 instr)
{
    const u32 opcode = bits(instr, 24, 21);
    const bool set_flags = bit(instr, 20);
    const u32 rn = bits(instr, 19, 16);
    const u32 rd = bits(instr, 15, 12);
    const bool register_shift = !bit(instr, 25) && bit(instr, 4);

    bool shifter_carry = false;
    const u32 op2 = shifter_operand(instr, shifter_carry);
    const u32 op1 = regs_[rn] + ((rn == 15 && register_shift) ? 4 : 0);
    const bool carry = cpsr_ & psr::C;

    // Logical ops take C from the barrel shifter; arithmetic ops take C and V from the adder.
    u32 result = 0;
    bool logical = true;
    AluResult arith{};

    switch (opcode)
    {
        case 0x0: // AND
        case 0x8: // TST
            result = op1 & op2;
            break;
        case 0x1: // EOR
        case 0x9: // TEQ
            result = op1 ^ op2;
            break;
        case 0xC: // ORR
            result = op1 | op2;
            break;
        case 0xD: // MOV
            result = op2;
            break;
        case 0xE: // BIC
            result = op1 & ~op2;
            break;
        case 0xF: // MVN
            result = ~op2;
            break;
        case 0x2: // SUB
        case 0xA: // CMP
            arith = add_with_carry(op1, ~op2, true);
            logical = false;
            break;
        case 0x3: // RSB
            arith = add_with_carry(op2, ~op1, true);
            logical = false;
            break;
        case 0x4: // ADD
        case 0xB: // CMN
            arith = add_with_carry(op1, op2, false);
            logical = false;
            break;
        case 0x5: // ADC
            arith = add_with_carry(op1, op2, carry);
            logical = false;
            break;
        case 0x6: // SBC
            arith = add_with_carry(op1, ~op2, carry);
            logical = false;
            break;
        case 0x7: // RSC
            arith = add_with_carry(op2, ~op1, carry);
            logical = false;
            break;
    }
    if (!logical)
    {
        result = arith.value;
    }

    // TST, TEQ, CMP and CMN only update flags; they never write Rd.
    const bool is_test = opcode >= 0x8 && opcode <= 0xB;

    if (set_flags)
    {
        if (rd == 15 && !is_test)
        {
            // Writing PC with S set is the exception-return idiom (e.g. SUBS pc, lr, #4):
            // it restores CPSR from the current mode's SPSR instead of setting flags.
            set_cpsr(spsr());
        }
        else
        {
            set_nz(result);
            cpsr_ &= ~psr::C;
            if (logical ? shifter_carry : arith.carry)
            {
                cpsr_ |= psr::C;
            }
            if (!logical)
            {
                cpsr_ &= ~psr::V;
                if (arith.overflow)
                {
                    cpsr_ |= psr::V;
                }
            }
        }
    }

    int cycles = 1 + (register_shift ? 1 : 0);
    if (!is_test)
    {
        regs_[rd] = result;
        if (rd == 15)
        {
            flush_pipeline();
            cycles += 2;
        }
    }
    return cycles;
}

int Arm7tdmi::arm_mrs(u32 instr)
{
    // Bit 22 selects SPSR (1) or CPSR (0).
    regs_[bits(instr, 15, 12)] = bit(instr, 22) ? spsr() : cpsr_;
    return 1;
}

int Arm7tdmi::arm_msr(u32 instr)
{
    const u32 value = bit(instr, 25) ? ror32(instr & 0xFF, bits(instr, 11, 8) * 2) : regs_[instr & 0xF];

    // Bits 19-16 choose which bytes of the PSR to write, so code can update just the
    // flags without disturbing the mode, or vice versa.
    u32 mask = 0;
    if (bit(instr, 19))
    {
        mask |= 0xFF000000; // f: condition flags
    }
    if (bit(instr, 18))
    {
        mask |= 0x00FF0000; // s: status (reserved on ARMv4)
    }
    if (bit(instr, 17))
    {
        mask |= 0x0000FF00; // x: extension (reserved on ARMv4)
    }
    if (bit(instr, 16))
    {
        mask |= 0x000000FF; // c: control (mode, I, F, T)
    }

    if (bit(instr, 22))
    {
        set_spsr((spsr() & ~mask) | (value & mask));
    }
    else
    {
        // Unprivileged code may only change the condition flags.
        if (mode() == CpuMode::User)
        {
            mask &= 0xFF000000;
        }
        set_cpsr((cpsr_ & ~mask) | (value & mask));
    }
    return 1;
}

int Arm7tdmi::arm_single_transfer(u32 instr)
{
    const bool pre_index = bit(instr, 24);  // Apply the offset before (1) or after (0) the access.
    const bool up = bit(instr, 23);         // Add (1) or subtract (0) the offset.
    const bool byte = bit(instr, 22);       // Transfer a byte (1) or a word (0).
    const bool write_back = bit(instr, 21); // Write the offset address back into Rn.
    const bool load = bit(instr, 20);       // LDR (1) or STR (0).
    const u32 rn = bits(instr, 19, 16);
    const u32 rd = bits(instr, 15, 12);

    // The offset is either a 12-bit immediate or a register shifted by an immediate amount.
    u32 offset;
    if (bit(instr, 25))
    {
        bool unused_carry = false;
        offset = shift_by_immediate(bits(instr, 6, 5), regs_[instr & 0xF], bits(instr, 11, 7), cpsr_ & psr::C,
                                    unused_carry);
    }
    else
    {
        offset = instr & 0xFFF;
    }

    const u32 base = regs_[rn];
    const u32 offset_addr = up ? base + offset : base - offset;
    const u32 addr = pre_index ? offset_addr : base;

    // Post-indexed transfers always write back; pre-indexed ones only when W is set.
    const bool do_write_back = !pre_index || write_back;

    if (load)
    {
        u32 value;
        if (byte)
        {
            value = bus_.read8(addr);
        }
        else
        {
            // An unaligned word load reads the aligned word, then rotates it so the
            // addressed byte lands in bits 0-7. Some games rely on this quirk.
            value = ror32(bus_.read32(addr), (addr & 3) * 8);
        }
        if (do_write_back)
        {
            regs_[rn] = offset_addr;
        }
        // Assign Rd after the write-back so that, when Rn == Rd, the loaded value wins.
        regs_[rd] = value;
        if (rd == 15)
        {
            flush_pipeline();
            return 5;
        }
        return 3;
    }

    // Storing r15 stores the instruction address + 12, one word further than a normal read.
    const u32 value = regs_[rd] + (rd == 15 ? 4 : 0);
    if (byte)
    {
        bus_.write8(addr, static_cast<u8>(value));
    }
    else
    {
        bus_.write32(addr, value);
    }
    if (do_write_back)
    {
        regs_[rn] = offset_addr;
    }
    return 2;
}

}
