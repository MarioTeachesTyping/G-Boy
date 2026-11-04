// ================ //
// I/O Register Map //
// ================ //

// Offsets of the memory-mapped hardware registers, relative to 0x04000000.
// Only registers that something in the core uses are listed; more get added
// as each subsystem comes online.

#pragma once

#include "core/common.hpp"

namespace gboy::io
{

// Display control: video mode, which layers are enabled, forced blank, etc.
inline constexpr u32 DISPCNT = 0x000;

// Display status: VBlank/HBlank/VCount flags (read-only) plus IRQ enables and the VCount target.
inline constexpr u32 DISPSTAT = 0x004;

// The scanline currently being drawn (0-227). Read-only.
inline constexpr u32 VCOUNT = 0x006;

// Button state, one bit per key, 0 = pressed (active-low).
inline constexpr u32 KEYINPUT = 0x130;

// Interrupt enable mask: which interrupt sources are allowed to fire.
inline constexpr u32 IE = 0x200;

// Interrupt request flags: which interrupts are pending. Writing 1 to a bit acknowledges it.
inline constexpr u32 IF = 0x202;

// Game Pak / SRAM wait-state configuration, which changes memory access timing.
inline constexpr u32 WAITCNT = 0x204;

// Interrupt master enable: a global on/off switch for all interrupts.
inline constexpr u32 IME = 0x208;

}
