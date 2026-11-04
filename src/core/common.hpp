// ========================== //
// Common Types & Bit Helpers //
// ========================== //

// Fixed-width integer aliases and small bit-twiddling helpers shared by
// every part of the emulator core.

#pragma once

#include <cstddef>
#include <cstdint>

namespace gboy
{

// Short fixed-width integer names, used everywhere hardware register sizes matter.
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

// Returns whether bit `n` of `value` is set.
constexpr bool bit(u32 value, unsigned n)
{
    return (value >> n) & 1u;
}

// Extracts the inclusive bit range [hi:lo] of `value` and shifts it down to bit 0.
// This is how instruction fields like "bits 15-12 = Rd" are pulled out of opcodes.
constexpr u32 bits(u32 value, unsigned hi, unsigned lo)
{
    return (value >> lo) & ((hi - lo == 31) ? 0xFFFFFFFFu : ((1u << (hi - lo + 1)) - 1));
}

// Rotates `value` right by `amount` bits (mod 32), matching the ARM ROR operation.
constexpr u32 ror32(u32 value, unsigned amount)
{
    amount &= 31;
    return amount == 0 ? value : (value >> amount) | (value << (32 - amount));
}

// Treats the low `width` bits of `value` as a signed number and widens it to 32 bits.
// Used for branch offsets and other signed immediates packed into opcodes.
constexpr s32 sign_extend(u32 value, unsigned width)
{
    const unsigned shift = 32 - width;
    return static_cast<s32>(value << shift) >> shift;
}

}
