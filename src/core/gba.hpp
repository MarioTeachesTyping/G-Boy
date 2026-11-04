// ======================= //
// Game Boy Advance System //
// ======================= //

// The top-level console object that owns and wires together the cartridge,
// bus, CPU and PPU. This is the public API every frontend talks to.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/cartridge/cartridge.hpp"
#include "core/common.hpp"
#include "core/cpu/arm7tdmi.hpp"
#include "core/memory/bus.hpp"
#include "core/ppu/ppu.hpp"

namespace gboy
{

// A complete Game Boy Advance. Load a ROM, then call run_frame() once per host frame.
//
// The components are heap-allocated because the bus alone holds ~500 KB of RAM
// arrays, which would overflow the stack. That keeps Gba itself small enough to
// create as a local variable.
class Gba
{
public:
    // Builds all components and performs an initial reset.
    Gba();

    // Inserts a cartridge and resets the console. Returns false and fills `error` if the
    // ROM is unusable.
    bool load_rom(std::vector<u8> rom, std::string& error);

    // Installs a 16 KB BIOS image and resets so the console boots through it.
    bool load_bios(const std::vector<u8>& bios, std::string& error);

    // Power-cycles the console. Without a BIOS this jumps straight into the cartridge.
    void reset();

    // Puts the CPU in the state the real BIOS leaves behind after its boot animation
    // (stack pointers set, System mode) and jumps to the cartridge entry point 0x08000000.
    void skip_bios();

    // Executes a single CPU instruction and advances the other components by the
    // same number of cycles. Returns the cycles used.
    int step();

    // Runs until the PPU completes a frame, or until the CPU halts on an error.
    void run_frame();

    // Access to the individual components, for frontends, tests and the future debugger.
    Arm7tdmi& cpu()
    {
        return *cpu_;
    }

    Bus& bus()
    {
        return *bus_;
    }

    Ppu& ppu()
    {
        return *ppu_;
    }

    const Cartridge& cartridge() const
    {
        return *cart_;
    }

    // Total CPU cycles executed since the last reset.
    u64 cycles() const
    {
        return cycles_;
    }

    // Whether a BIOS image has been loaded.
    bool has_bios() const
    {
        return has_bios_;
    }

private:
    // The inserted Game Pak. Declared first because the bus holds a reference to it.
    std::unique_ptr<Cartridge> cart_;

    // Memory map and RAM. The CPU and PPU both hold references to it.
    std::unique_ptr<Bus> bus_;

    // The ARM7TDMI processor.
    std::unique_ptr<Arm7tdmi> cpu_;

    // The graphics chip.
    std::unique_ptr<Ppu> ppu_;

    // Running cycle counter, used for timing and statistics.
    u64 cycles_ = 0;

    // True once load_bios() has succeeded; decides whether reset() boots via the BIOS.
    bool has_bios_ = false;
};

}
