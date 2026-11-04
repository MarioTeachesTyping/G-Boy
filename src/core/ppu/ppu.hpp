// ======================= //
// Picture Processing Unit //
// ======================= //

// Scanline timing (VCOUNT, HBlank/VBlank flags) and the renderer that turns
// VRAM into a 240x160 framebuffer. M0 renders video mode 3 only; the tiled
// modes, sprites and effects arrive in M4.

#pragma once

#include <array>

#include "core/common.hpp"

namespace gboy
{

class Bus;

// The GBA's graphics chip. It is driven by CPU cycles: each call to tick() moves
// the beam forward, and a line is rendered into the framebuffer when it enters HBlank.
class Ppu
{
public:
    // Visible screen size in pixels.
    static constexpr int kWidth = 240;
    static constexpr int kHeight = 160;

    // 160 visible lines plus 68 VBlank lines.
    static constexpr int kTotalLines = 228;

    // Each line takes 1232 cycles: 960 drawing pixels (HDraw) and 272 in HBlank.
    static constexpr int kCyclesPerLine = 1232;
    static constexpr int kHDrawCycles = 960;

    // One full frame is 280896 cycles, which gives ~59.73 frames per second at 16.78 MHz.
    static constexpr int kCyclesPerFrame = kCyclesPerLine * kTotalLines;

    // Creates a PPU that reads video memory and registers through `bus`.
    explicit Ppu(Bus& bus);

    // Returns to the start of line 0 and clears the framebuffer.
    void reset();

    // Advances the PPU by `cycles` CPU cycles, rendering lines and updating status
    // registers as boundaries are crossed.
    void tick(int cycles);

    // Returns the scanline currently being processed (0-227).
    int line() const
    {
        return line_;
    }

    // Returns how many frames have been completed. Increments on entering VBlank.
    u64 frame_count() const
    {
        return frame_count_;
    }

    // Returns the most recently drawn image in BGR555 (the GBA's native colour format),
    // stored row by row from the top-left corner.
    const std::array<u16, kWidth * kHeight>& framebuffer() const
    {
        return framebuffer_;
    }

private:
    // Handles the switch from HDraw to HBlank: renders the finished line and raises the flag.
    void enter_hblank();

    // Moves to the next scanline, wrapping to 0 after line 227 and counting frames at VBlank.
    void next_line();

    // Draws the current line into the framebuffer according to DISPCNT.
    void render_line();

    // Writes VCOUNT and the VBlank/HBlank/VCount-match bits of DISPSTAT for the current state.
    void update_status_flags();

    // Bus used to read VRAM/palette and to update the PPU's I/O registers.
    Bus& bus_;

    // Scanline currently being processed.
    int line_ = 0;

    // Cycles spent so far on the current line.
    int line_cycles_ = 0;

    // Whether the current line has already entered HBlank.
    bool in_hblank_ = false;

    // Number of frames completed since reset.
    u64 frame_count_ = 0;

    // The output image, one BGR555 value per pixel.
    std::array<u16, kWidth * kHeight> framebuffer_{};
};

}
