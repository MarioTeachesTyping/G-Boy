# `src/core/memory/` — System Bus & I/O Registers

Every memory access in the GBA (CPU instruction fetches, loads, stores, and later DMA) goes through
the **bus**. Its job is to look at an address and decide which piece of hardware answers it.

## Files

| File | What's in it |
|---|---|
| [`bus.hpp`](bus.hpp) / [`bus.cpp`](bus.cpp) | The `Bus` class: owns all RAM regions, decodes addresses, applies mirroring and I/O write rules. |
| [`io_regs.hpp`](io_regs.hpp) | Named offsets for the memory-mapped hardware registers (`DISPCNT`, `VCOUNT`, `KEYINPUT`, `IE`, ...). |

## The Memory Map

The top byte of a 32-bit address selects the region:

| Address | Size | Region | Notes |
|---|---|---|---|
| `0x00000000` | 16 KB | BIOS | Read-only. Empty unless a BIOS file is loaded. |
| `0x02000000` | 256 KB | EWRAM (external work RAM) | Big but slow (16-bit bus). |
| `0x03000000` | 32 KB | IWRAM (internal work RAM) | Small and fast (32-bit bus); games put hot code and the stack here. |
| `0x04000000` | 1 KB | I/O registers | Controls every piece of hardware: display, sound, timers, DMA, buttons, interrupts. |
| `0x05000000` | 1 KB | Palette RAM | 512 colours, 15-bit BGR555. |
| `0x06000000` | 96 KB | VRAM | Tiles, tile maps, bitmaps, sprite graphics. |
| `0x07000000` | 1 KB | OAM | Attributes for the 128 sprites. |
| `0x08000000`–`0x0DFFFFFF` | up to 32 MB ×3 | Game Pak ROM | The same ROM appears three times, with different wait-state settings. |
| `0x0E000000` | 64 KB | Game Pak SRAM | Battery-backed save memory. |

### Mirroring

Most regions are smaller than their address window, so they repeat. EWRAM reappears every 256 KB,
IWRAM every 32 KB, and so on. The bus implements this by masking the address with `size - 1`.

VRAM is the odd one out. It repeats every 128 KB, but only 96 KB physically exist, so the top 32 KB of
each 128 KB block mirrors the 32 KB at offset `0x10000`. `vram_offset()` handles that.

## Reads And Writes

- `read8/16/32` and `write8/16/32` are **little-endian** and **force-aligned**: a 16-bit access ignores
  bit 0 of the address and a 32-bit access ignores bits 0–1. The CPU's special handling of misaligned
  loads (rotation) happens in the CPU, not here.
- Writes to ROM, BIOS, or unmapped addresses are silently ignored, like on hardware.
- Unmapped reads currently return `0`. Real hardware returns "open bus" (left-over data from the last
  fetch), which arrives in M3.

## I/O Registers

I/O registers are stored as a raw 1 KB byte array, but CPU writes go through `io_write8()`, which
enforces each register's rules:

- **`VCOUNT`** (current scanline) is read-only, so CPU writes are dropped.
- **`DISPSTAT`** bits 0–2 (VBlank / HBlank / VCount-match flags) are read-only status bits. The CPU
  can only change the upper bits.

Hardware components need to set registers the CPU can't, so they use **`io_set16()`**, which bypasses
the rules. The PPU uses it to update `VCOUNT` and the `DISPSTAT` flags every line. `io_read16()` reads
a register without side effects.

As more hardware comes online (M3), `io_write8()` gains side effects: writing 1 to an `IF` bit
acknowledges an interrupt, writing a DMA control register starts a transfer, and so on.

## Adding A Register

1. Add a named offset to `io_regs.hpp`, with a one-sentence comment saying what it does.
2. If CPU writes need special behaviour, add a `case` to `Bus::io_write8()`.
3. If hardware updates it, have that component call `io_set16()`.
