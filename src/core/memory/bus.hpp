// ========== //
// System Bus //
// ========== //

// Address decoding for the whole GBA memory map. Owns every RAM region
// (BIOS, EWRAM, IWRAM, I/O, palette, VRAM, OAM, SRAM) and forwards Game Pak
// ROM reads to the cartridge.

#pragma once

#include <array>
#include <vector>

#include "core/common.hpp"

namespace gboy
{

class Cartridge;

// The system bus. Every CPU load/store goes through here, and other components
// (PPU, later DMA) use it to reach the memory they care about.
//
// All multi-byte accesses are little-endian and force-aligned (addr & ~1 for
// halfwords, addr & ~3 for words). ARM's "rotated unaligned LDR" behaviour is
// handled by the CPU, not here.
class Bus
{
public:
    // Sizes of each memory region in bytes. Regions are mirrored across their
    // address window, so these also serve as the mirroring masks.
    static constexpr std::size_t kBiosSize = 16 * 1024;
    static constexpr std::size_t kEwramSize = 256 * 1024;
    static constexpr std::size_t kIwramSize = 32 * 1024;
    static constexpr std::size_t kIoSize = 1024;
    static constexpr std::size_t kPaletteSize = 1024;
    static constexpr std::size_t kVramSize = 96 * 1024;
    static constexpr std::size_t kOamSize = 1024;
    static constexpr std::size_t kSramSize = 64 * 1024;

    // Creates a bus attached to `cart`. The cartridge must outlive the bus.
    explicit Bus(const Cartridge& cart);

    // Clears all volatile RAM and I/O registers. The BIOS and battery-backed SRAM are kept.
    void reset();

    // Copies a BIOS image into the BIOS region. Returns false if it is not exactly 16 KB.
    bool load_bios(const std::vector<u8>& bios);

    // Reads 8, 16 or 32 bits from `addr`. Unmapped addresses currently read as 0.
    u8 read8(u32 addr) const;
    u16 read16(u32 addr) const;
    u32 read32(u32 addr) const;

    // Writes 8, 16 or 32 bits to `addr`. Writes to ROM, BIOS or unmapped space are ignored.
    void write8(u32 addr, u8 value);
    void write16(u32 addr, u16 value);
    void write32(u32 addr, u32 value);

    // Gives the PPU (and later the debugger) direct read access to video memory
    // without going through address decoding for every pixel.
    const std::array<u8, kVramSize>& vram() const
    {
        return vram_;
    }

    // Direct read access to palette RAM (512 BG colours + 512 sprite colours, BGR555).
    const std::array<u8, kPaletteSize>& palette() const
    {
        return palette_;
    }

    // Direct read access to Object Attribute Memory (sprite definitions).
    const std::array<u8, kOamSize>& oam() const
    {
        return oam_;
    }

    // Reads a 16-bit I/O register by its offset from 0x04000000, with no side effects.
    u16 io_read16(u32 offset) const;

    // Sets a 16-bit I/O register directly, bypassing write rules. Hardware components use
    // this to update registers the CPU can only read (e.g. the PPU writing VCOUNT).
    void io_set16(u32 offset, u16 value);

private:
    // Translates `addr` into a pointer to its backing byte for regions that are plain
    // writable memory. Returns nullptr for read-only (ROM/BIOS), I/O, or unmapped space.
    u8* writable_byte(u32 addr);

    // Applies a CPU byte write to an I/O register, honouring read-only bits and,
    // in later milestones, write side effects (IRQ acknowledge, DMA start, ...).
    void io_write8(u32 offset, u8 value);

    // The cartridge that answers reads in the Game Pak ROM window.
    const Cartridge& cart_;

    // 16 KB system ROM at 0x00000000. Left zeroed when booting without a BIOS.
    std::array<u8, kBiosSize> bios_{};

    // 256 KB external work RAM at 0x02000000 (slower, 16-bit bus).
    std::array<u8, kEwramSize> ewram_{};

    // 32 KB internal work RAM at 0x03000000 (fast, 32-bit bus; games put hot code here).
    std::array<u8, kIwramSize> iwram_{};

    // Raw backing store for the 1 KB of memory-mapped I/O registers at 0x04000000.
    std::array<u8, kIoSize> io_{};

    // 1 KB palette RAM at 0x05000000.
    std::array<u8, kPaletteSize> palette_{};

    // 96 KB video RAM at 0x06000000 (tiles, maps and bitmaps).
    std::array<u8, kVramSize> vram_{};

    // 1 KB sprite attribute memory at 0x07000000.
    std::array<u8, kOamSize> oam_{};

    // 64 KB battery-backed save RAM at 0x0E000000 (Flash/EEPROM support comes in M7).
    std::array<u8, kSramSize> sram_{};
};

}
