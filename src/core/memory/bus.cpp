// ========== //
// System Bus //
// ========== //

// Memory map decoding, region mirroring, and I/O register write rules.

#include "core/memory/bus.hpp"

#include <algorithm>

#include "core/cartridge/cartridge.hpp"
#include "core/memory/io_regs.hpp"

namespace gboy
{

namespace
{

// Returns the top byte of an address, which selects the memory region on the GBA
// (0x02 = EWRAM, 0x06 = VRAM, 0x08 = ROM, ...).
constexpr u32 region_of(u32 addr)
{
    return addr >> 24;
}

// Maps an address in the VRAM window to an offset inside the 96 KB VRAM array.
// VRAM repeats every 128 KB, but only 96 KB physically exist, so the top 32 KB of
// each 128 KB block mirrors the 32 KB at 0x10000 (the sprite tile area).
constexpr u32 vram_offset(u32 addr)
{
    u32 offset = addr & 0x1FFFF;
    if (offset >= 0x18000)
    {
        offset -= 0x8000;
    }
    return offset;
}

}

Bus::Bus(const Cartridge& cart) : cart_(cart)
{
}

void Bus::reset()
{
    ewram_.fill(0);
    iwram_.fill(0);
    io_.fill(0);
    palette_.fill(0);
    vram_.fill(0);
    oam_.fill(0);
}

bool Bus::load_bios(const std::vector<u8>& bios)
{
    if (bios.size() != kBiosSize)
    {
        return false;
    }
    std::copy(bios.begin(), bios.end(), bios_.begin());
    return true;
}

u8 Bus::read8(u32 addr) const
{
    switch (region_of(addr))
    {
        case 0x00:
            return addr < kBiosSize ? bios_[addr] : 0;
        case 0x02:
            return ewram_[addr & (kEwramSize - 1)];
        case 0x03:
            return iwram_[addr & (kIwramSize - 1)];
        case 0x04:
        {
            const u32 offset = addr & 0x00FFFFFF;
            return offset < kIoSize ? io_[offset] : 0;
        }
        case 0x05:
            return palette_[addr & (kPaletteSize - 1)];
        case 0x06:
            return vram_[vram_offset(addr)];
        case 0x07:
            return oam_[addr & (kOamSize - 1)];
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
            // Three 32 MB mirrors of the same ROM, differing only in wait states.
            return cart_.read8(addr & 0x01FFFFFF);
        case 0x0E:
        case 0x0F:
            return sram_[addr & (kSramSize - 1)];
        default:
            // TODO(M3): real hardware returns "open bus" (the last prefetched opcode).
            return 0;
    }
}

u16 Bus::read16(u32 addr) const
{
    addr &= ~1u;
    return static_cast<u16>(read8(addr) | (read8(addr + 1) << 8));
}

u32 Bus::read32(u32 addr) const
{
    addr &= ~3u;
    return static_cast<u32>(read16(addr)) | (static_cast<u32>(read16(addr + 2)) << 16);
}

u8* Bus::writable_byte(u32 addr)
{
    switch (region_of(addr))
    {
        case 0x02:
            return &ewram_[addr & (kEwramSize - 1)];
        case 0x03:
            return &iwram_[addr & (kIwramSize - 1)];
        case 0x05:
            return &palette_[addr & (kPaletteSize - 1)];
        case 0x06:
            return &vram_[vram_offset(addr)];
        case 0x07:
            return &oam_[addr & (kOamSize - 1)];
        case 0x0E:
        case 0x0F:
            return &sram_[addr & (kSramSize - 1)];
        default:
            return nullptr;
    }
}

void Bus::write8(u32 addr, u8 value)
{
    if (region_of(addr) == 0x04)
    {
        const u32 offset = addr & 0x00FFFFFF;
        if (offset < kIoSize)
        {
            io_write8(offset, value);
        }
        return;
    }

    // TODO(M4): byte writes to palette/VRAM really write the byte to both halves of the
    // halfword, and byte writes to OAM are ignored entirely.
    if (u8* p = writable_byte(addr))
    {
        *p = value;
    }
}

void Bus::write16(u32 addr, u16 value)
{
    addr &= ~1u;
    write8(addr, static_cast<u8>(value));
    write8(addr + 1, static_cast<u8>(value >> 8));
}

void Bus::write32(u32 addr, u32 value)
{
    addr &= ~3u;
    write16(addr, static_cast<u16>(value));
    write16(addr + 2, static_cast<u16>(value >> 16));
}

u16 Bus::io_read16(u32 offset) const
{
    return static_cast<u16>(io_[offset] | (io_[offset + 1] << 8));
}

void Bus::io_set16(u32 offset, u16 value)
{
    io_[offset] = static_cast<u8>(value);
    io_[offset + 1] = static_cast<u8>(value >> 8);
}

void Bus::io_write8(u32 offset, u8 value)
{
    switch (offset)
    {
        case io::DISPSTAT:
            // Bits 0-2 are status flags driven by the PPU; the CPU cannot change them.
            io_[offset] = static_cast<u8>((io_[offset] & 0x07) | (value & ~0x07));
            return;
        case io::VCOUNT:
        case io::VCOUNT + 1:
            // VCOUNT is read-only; only the PPU updates it.
            return;
        default:
            // TODO(M3): IF acknowledge-on-write, timers, DMA, KEYINPUT, etc.
            io_[offset] = value;
            return;
    }
}

}
