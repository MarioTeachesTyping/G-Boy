# `src/core/` — The Emulator Core

This folder is the Game Boy Advance itself. It compiles into the `gboy_core` static library and
has no dependencies beyond the C++20 standard library.

## Files At This Level

| File | Purpose |
|---|---|
| [`gba.hpp`](gba.hpp) / [`gba.cpp`](gba.cpp) | **`gboy::Gba`**, the whole console in one object, and the only class a frontend really needs. |
| [`common.hpp`](common.hpp) | Integer aliases (`u8`, `u16`, `u32`, `s32`, ...) and bit helpers (`bit`, `bits`, `ror32`, `sign_extend`) used everywhere. |

## Subfolders

| Folder | Hardware it emulates |
|---|---|
| [`cpu/`](cpu/) | The ARM7TDMI processor: registers, modes, exceptions, and the ARM instruction set. |
| [`memory/`](memory/) | The system bus, which decodes every address into BIOS, RAM, I/O registers, video memory or the cartridge. |
| [`cartridge/`](cartridge/) | The Game Pak: the ROM image and its header. Save memory comes later. |
| [`ppu/`](ppu/) | The Picture Processing Unit (graphics chip): scanline timing and the renderer. |

## How The Pieces Connect

```
Gba
 ├── Cartridge      owns the ROM bytes
 ├── Bus            owns all RAM; reads ROM through the Cartridge
 ├── Arm7tdmi       does every fetch/load/store through the Bus
 └── Ppu            reads VRAM/palette and updates display registers through the Bus
```

`Gba` creates these in that order, because each one holds a reference to the one before it. They
are heap-allocated (`std::unique_ptr`) because the bus alone holds about 500 KB of memory arrays,
which would overflow the stack if `Gba` contained them directly.

## The Run Loop

```cpp
gboy::Gba gba;
std::string error;
gba.load_rom(rom_bytes, error);   // also resets the console
gba.run_frame();                  // runs ~280,896 CPU cycles (one 59.73 Hz frame)
const auto& pixels = gba.ppu().framebuffer();  // 240x160 BGR555
```

Inside, `Gba::step()` is the heartbeat:

1. The CPU executes **one instruction** and reports how many cycles it took.
2. The PPU is advanced by that many cycles, so it renders lines and flips HBlank/VBlank at the right moments.
3. A running cycle counter is updated.

`run_frame()` just repeats `step()` until the PPU reports a new frame, or until the CPU halts on
an error.

Timers, DMA and interrupts (M3) will hook into this loop through an event scheduler, so
components don't all have to be ticked on every instruction.

## Booting With And Without A BIOS

The real console starts by running Nintendo's 16 KB BIOS, which shows the logo and then jumps to
the cartridge. That BIOS is copyrighted, so G-Boy never ships it:

- **With a BIOS** (`load_bios()`): the CPU starts at address `0x00000000` like real hardware, and
  BIOS calls (`SWI`) work.
- **Without a BIOS** (the default): `Gba::skip_bios()` sets up the registers exactly as the BIOS
  would leave them (stack pointers for each mode, System mode) and starts at the cartridge entry
  point `0x08000000`. Games run straight away, but any BIOS call stops the CPU with a clear error
  until high-level BIOS emulation arrives in M3.

## Errors

The core never throws in the hot path. When the CPU meets something it can't execute (Thumb code
before M2, or a BIOS call with no BIOS), it **halts** and records a message such as
`SWI 0x06 called without a BIOS ... at 0x08001E18`. Frontends check `gba.cpu().halted()` and
`gba.cpu().error()`. The same hook will power "break on error" in the debugger.
