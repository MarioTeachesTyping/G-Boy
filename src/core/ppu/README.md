# `src/core/ppu/` — Picture Processing Unit

The PPU is the GBA's graphics chip. It reads video memory and turns it into a **240×160** image,
one scanline at a time, about 59.73 times a second.

## Files

| File | What's in it |
|---|---|
| [`ppu.hpp`](ppu.hpp) | The `Ppu` class and the timing constants. |
| [`ppu.cpp`](ppu.cpp) | The scanline timing state machine and the renderer. |

## Timing

The PPU is driven by CPU cycles. After each CPU instruction, `Gba::step()` calls
`ppu.tick(cycles)`, and the PPU moves its virtual "beam" forward by that much:

```
 one scanline = 1232 cycles
 |<------- HDraw: 960 cycles ------->|<-- HBlank: 272 -->|

 one frame = 228 scanlines = 280,896 cycles
 lines 0-159   : visible (drawn)
 lines 160-227 : VBlank (nothing drawn; games update graphics here)
```

As boundaries are crossed:

- **Entering HBlank:** the line that just finished is rendered into the framebuffer, and the
  HBlank flag in `DISPSTAT` is set.
- **Starting a new line:** `VCOUNT` is updated. On reaching line 160 the frame counter increments
  (this is what `Gba::run_frame()` waits for) and the VBlank flag is set.
- **VCount match:** if `VCOUNT` equals the target number a game wrote into `DISPSTAT` bits 8–15,
  the VCount flag is set.

One hardware quirk is reproduced: the VBlank flag clears on line 227, one line *before* the frame
wraps back to line 0.

The interrupts these events trigger (VBlank / HBlank / VCount IRQs) and the DMA transfers they
start come in M3. The `TODO(M3)` comments in `ppu.cpp` mark where they go.

## Rendering

`render_line()` looks at `DISPCNT` (the display control register) to decide what to draw:

| Situation | Result |
|---|---|
| Forced blank (bit 7 set) | The line is white. Games do this while loading graphics. |
| **Mode 3** with background 2 enabled | VRAM is read as a 240×160 array of 16-bit colours and copied straight to the screen. |
| Anything else | The line is filled with the backdrop colour (palette entry 0). |

Mode 3 is the simplest GBA video mode, so it's enough to see a test program draw something. The
real work is in **M4**:

- **Modes 0–2: tiled backgrounds.** The screen is built from 8×8 tiles, with up to four scrolling
  layers, some of which can rotate and scale.
- **Modes 4–5:** more bitmap modes (palettised, and double-buffered).
- **Sprites:** 128 objects with priority, flipping and rotation/scaling.
- **Effects:** windows, alpha blending, brightness fades, mosaic.

## The Framebuffer

`framebuffer()` returns 240×160 `u16` pixels, row by row from the top-left, in the GBA's native
**BGR555** format:

```
 bit: 15 | 14..10 | 9..5  | 4..0
      -- |  blue  | green |  red
```

Frontends convert it to whatever they need. The CLI's `--dump-ppm` expands each 5-bit channel to
8 bits and writes a PPM image.
