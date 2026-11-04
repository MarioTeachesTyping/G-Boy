// ======================= //
// Picture Processing Unit //
// ======================= //

// Beam timing state machine and the scanline renderer.

#include "core/ppu/ppu.hpp"

#include "core/memory/bus.hpp"
#include "core/memory/io_regs.hpp"

namespace gboy
{

namespace
{

// DISPSTAT status bits (all read-only to the CPU).
constexpr u16 kVBlankFlag = 1 << 0; // Set while lines 160-226 are being "drawn".
constexpr u16 kHBlankFlag = 1 << 1; // Set during the HBlank part of every line.
constexpr u16 kVCountFlag = 1 << 2; // Set when VCOUNT equals the target in DISPSTAT bits 8-15.

}

Ppu::Ppu(Bus& bus) : bus_(bus)
{
}

void Ppu::reset()
{
    line_ = 0;
    line_cycles_ = 0;
    in_hblank_ = false;
    frame_count_ = 0;
    framebuffer_.fill(0);
    update_status_flags();
}

void Ppu::tick(int cycles)
{
    line_cycles_ += cycles;

    // Loop because a large tick can cross several boundaries at once.
    while (true)
    {
        if (!in_hblank_ && line_cycles_ >= kHDrawCycles)
        {
            enter_hblank();
        }
        else if (line_cycles_ >= kCyclesPerLine)
        {
            line_cycles_ -= kCyclesPerLine;
            next_line();
        }
        else
        {
            break;
        }
    }
}

void Ppu::enter_hblank()
{
    in_hblank_ = true;
    if (line_ < kHeight)
    {
        render_line();
    }
    update_status_flags();
    // TODO(M3): raise the HBlank IRQ and trigger HBlank DMA.
}

void Ppu::next_line()
{
    in_hblank_ = false;
    ++line_;
    if (line_ == kHeight)
    {
        ++frame_count_;
        // TODO(M3): raise the VBlank IRQ and trigger VBlank DMA.
    }
    else if (line_ == kTotalLines)
    {
        line_ = 0;
    }
    update_status_flags();
}

void Ppu::update_status_flags()
{
    bus_.io_set16(io::VCOUNT, static_cast<u16>(line_));

    u16 stat = bus_.io_read16(io::DISPSTAT) & ~(kVBlankFlag | kHBlankFlag | kVCountFlag);

    // The VBlank flag is cleared one line early, on line 227, which is a documented hardware quirk.
    if (line_ >= kHeight && line_ < kTotalLines - 1)
    {
        stat |= kVBlankFlag;
    }
    if (in_hblank_)
    {
        stat |= kHBlankFlag;
    }
    if (line_ == (stat >> 8))
    {
        stat |= kVCountFlag;
    }
    bus_.io_set16(io::DISPSTAT, stat);
}

void Ppu::render_line()
{
    const u16 dispcnt = bus_.io_read16(io::DISPCNT);
    const u32 mode = dispcnt & 0x7;
    const bool forced_blank = dispcnt & (1 << 7);
    const bool bg2_enabled = dispcnt & (1 << 10);
    u16* row = &framebuffer_[static_cast<std::size_t>(line_) * kWidth];

    // Forced blank turns the whole screen white, so games can safely update VRAM.
    if (forced_blank)
    {
        for (int x = 0; x < kWidth; ++x)
        {
            row[x] = 0x7FFF;
        }
        return;
    }

    // Mode 3: VRAM is a 240x160 array of BGR555 pixels displayed on background layer 2.
    if (mode == 3 && bg2_enabled)
    {
        const auto& vram = bus_.vram();
        const std::size_t base = static_cast<std::size_t>(line_) * kWidth * 2;
        for (int x = 0; x < kWidth; ++x)
        {
            const std::size_t i = base + static_cast<std::size_t>(x) * 2;
            row[x] = static_cast<u16>((vram[i] | (vram[i + 1] << 8)) & 0x7FFF);
        }
        return;
    }

    // Any mode we can't draw yet shows the backdrop colour (palette entry 0), which is
    // also what the hardware shows when every layer is disabled.
    const auto& pal = bus_.palette();
    const u16 backdrop = static_cast<u16>((pal[0] | (pal[1] << 8)) & 0x7FFF);
    for (int x = 0; x < kWidth; ++x)
    {
        row[x] = backdrop;
    }
}

}
