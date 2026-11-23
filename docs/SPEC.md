# G-Boy — Game Boy Advance Emulator Spec & Plan

Status: **M0 done; M1 done except one Thumb-dependent test.** jsmolka `arm.gba` passes every ARM test (r12 = 0) when its single Thumb interworking test (#51) is skipped; the unmodified ROM will pass once M2 lands. armwrestler not yet run. Everything after M1 is planned work.

## 1. Goals

- Run commercial and homebrew GBA ROMs at full speed (59.73 fps) on Windows first, portable later.
- Clean, testable C++20 core with **no platform dependencies** — the core is a library; frontends
  (CLI, SDL window, later the debugger UI) sit on top of it.
- Correctness first, then speed. Every subsystem gets unit tests plus public test ROMs.
- Keep the door open for the "twist" (a user-facing CPU debugger) by making CPU state, memory,
  and stepping fully inspectable through the core API — but no debugger work yet.

## Non-Goals (For Now)

- Game Boy / Game Boy Color backwards compatibility (the GBA's Z80-ish co-processor).
- Link cable, GBA peripherals (e-Reader, solar sensor, rumble, RTC beyond what saves need).
- JIT / dynarec. An interpreter is plenty fast for the GBA's 16.78 MHz CPU.
- Shipping a BIOS. Users supply their own `gba_bios.bin`, or we boot without one (HLE skip, see §6).

## 2. Hardware Summary (What We're Emulating)

| Part      | Details |
|-----------|---------|
| CPU       | ARM7TDMI @ 16.78 MHz (2^24 Hz). ARMv4T: 32-bit ARM + 16-bit Thumb instruction sets, 3-stage pipeline, 7 modes, banked registers. |
| Display   | 240×160, 15-bit BGR555. 6 video modes (0–2 tiled, 3–5 bitmap), 4 BG layers, 128 sprites, windows, blending, mosaic. |
| Timing    | 1232 cycles per scanline (960 visible + 272 HBlank), 228 lines (160 visible + 68 VBlank) = 280 896 cycles/frame. |
| Sound     | 4 legacy GB channels + 2 Direct Sound FIFO channels (8-bit PCM via DMA/timers). |
| Other     | 4 DMA channels, 4 timers, interrupt controller, keypad, serial, wait-state control. |

### Memory Map

| Range                    | Size   | Region | Bus width |
|--------------------------|--------|--------|-----------|
| `0x00000000-0x00003FFF`  | 16 KB  | BIOS (read-protected outside BIOS) | 32 |
| `0x02000000-0x0203FFFF`  | 256 KB | EWRAM (on-board, mirrored every 256 KB) | 16 |
| `0x03000000-0x03007FFF`  | 32 KB  | IWRAM (on-chip, mirrored every 32 KB) | 32 |
| `0x04000000-0x040003FE`  | 1 KB   | I/O registers | 32 |
| `0x05000000-0x050003FF`  | 1 KB   | Palette RAM (mirrored) | 16 |
| `0x06000000-0x06017FFF`  | 96 KB  | VRAM (odd 128 KB mirroring) | 16 |
| `0x07000000-0x070003FF`  | 1 KB   | OAM (mirrored) | 32 |
| `0x08000000-0x0DFFFFFF`  | 3×32 MB| Game Pak ROM (wait states 0/1/2 mirrors) | 16 |
| `0x0E000000-0x0E00FFFF`  | 64 KB  | Game Pak SRAM / Flash | 8 |

## 3. Architecture

```
            +----------------------------------------------+
 frontend   |  cli (now)   sdl (M5)   debugger UI (later)  |
            +----------------------|-----------------------+
                                   | gboy::Gba public API
            +----------------------v-----------------------+
 core lib   |  Gba  (owns everything, run_frame / step)    |
 (gboy_core)|   ├─ Arm7tdmi  ──reads/writes──┐             |
            |   ├─ Ppu       ──reads VRAM────┤             |
            |   ├─ (Apu, Dma, Timers, Irq)   ▼             |
            |   └─ Bus  ── BIOS/EWRAM/IWRAM/IO/PAL/VRAM/OAM |
            |              └─ Cartridge (ROM + backup)      |
            +----------------------------------------------+
```

Directory Layout:

```
src/core/            platform-free emulator library (gboy_core)
  common.hpp         integer aliases, bit helpers
  gba.{hpp,cpp}      top-level system; owns components, drives the frame loop
  memory/bus.*       address decoding, memory regions, I/O register dispatch
  cartridge/cart.*   ROM loading, header parsing, (later) save backup types
  cpu/arm7tdmi.*     register file, modes, banking, exceptions, step()
  cpu/arm_isa.cpp    32-bit ARM instruction handlers
  cpu/thumb_isa.cpp  16-bit Thumb handlers (M2)
  ppu/ppu.*          scanline timing, DISPSTAT/VCOUNT, renderer
src/frontend/cli/    headless runner: header dump, run N frames, dump PPM
tests/               zero-dependency unit tests (tiny built-in harness)
docs/                this spec
```

### Key Design Decisions

1. **Core is a library, frontends are thin.** Nothing in `src/core` includes SDL, OS headers, or does I/O
   other than through byte buffers. This is what makes the future debugger cheap.
2. **Interpreter with decode-by-mask** to start. Once the ARM/Thumb ISA is complete and passes test ROMs,
   switch to precomputed lookup tables (4096-entry ARM table keyed on bits 27-20 & 7-4, 1024-entry Thumb table)
   for speed. Correctness first.
3. **Pipeline model:** R15 always reads as `current_instruction + 8` (ARM) / `+ 4` (Thumb). Branches call
   `flush_pipeline()`. We don't model prefetch-buffer contents explicitly unless a test demands it.
4. **Timing:** the scheduler is cycle-counted. M0 charges 1 cycle per instruction; M3 adds real
   N/S/I cycle costs and wait states (WAITCNT), plus an event scheduler (min-heap of timestamped events)
   for PPU line boundaries, timers, DMA, and audio sampling — avoiding per-cycle ticking of every component.
5. **Rendering is scanline-based** (render each line at HBlank start). Accurate enough for nearly all games;
   per-pixel mid-line effects are out of scope.
6. **Errors don't throw out of the hot path.** Unimplemented opcodes set a `halted_` flag and an error string
   the frontend can display. This doubles as the hook for "break on unknown instruction" in the debugger.

## 4. Milestones

| #  | Milestone | Scope | Done when |
|----|-----------|-------|-----------|
| M0 | **Scaffold** ✅ | Build system, memory map + bus, cartridge header parsing, ARM register file & mode banking, core ARM instrs (B/BL/BX, all data-processing, MRS/MSR, LDR/STR), PPU line timing + mode 3, CLI, unit tests | `make test` passes; CLI loads a ROM and prints the header |
| M1 | ARM ISA complete ✅* | Multiply / long multiply, halfword & signed transfers, LDM/STM (incl. user-bank + PC quirks), SWP, SWI/undefined exceptions, register-shift PC+12 quirk, unaligned-access rotation | jsmolka `arm.gba` passes, armwrestler ARM tests pass |
| M2 | Thumb ISA | All 19 Thumb formats, ARM↔Thumb interworking | jsmolka `thumb.gba` passes |
| M3 | System | IRQ controller (IE/IF/IME), HALT, timers, DMA (immediate/VBlank/HBlank/FIFO), keypad, WAITCNT cycle timing, event scheduler, BIOS boot + HLE SWI fallbacks | jsmolka `memory.gba`, `bios.gba` pass; real BIOS boots to the logo |
| M4 | Full PPU | Modes 0–5, affine BGs, sprites (incl. affine), priority, windows, alpha/brightness blending, mosaic | tonc demos render correctly; a handful of commercial games reach gameplay |
| M5 | SDL frontend | Window + scaling, keyboard/gamepad input, frame pacing, drag-and-drop ROMs | Games playable at 60 fps |
| M6 | Audio | GB channels 1–4, Direct Sound A/B FIFOs, resampling to host rate | Music sounds right in test games |
| M7 | Saves | Auto-detect SRAM / Flash 64K/128K / EEPROM 512B/8K, `.sav` persistence, save states | Saves survive restarts |
| M8 | Twist | CPU debugger UI (disassembler, breakpoints, memory viewer) — redesign TBD | — |

## 5. Testing Strategy

- **Unit tests** (`tests/`): hand-assembled instruction sequences loaded into a fake ROM, asserting registers,
  flags, memory and framebuffer contents. No external framework — keeps the build dependency-free.
- **Test ROMs** (not committed; place under `roms/test/`, git-ignored):
  - jsmolka/gba-tests — `arm`, `thumb`, `memory`, `bios`, `save`, `ppu`
  - armwrestler-gba-fixed
  - mGBA test suite (timing, DMA, IRQ edge cases — later milestones)
  - tonc examples for PPU features
- A future `--headless --frames N --dump-ppm` CLI path lets us screenshot-diff test ROM output in CI.

## 6. BIOS Policy

The Nintendo BIOS is copyrighted and is never committed. The CLI accepts `--bios <path>`. Without it,
`Gba::skip_bios()` sets the post-BIOS register state (SVC SP `0x03007FE0`, IRQ SP `0x03007FA0`,
SYS SP `0x03007F00`, CPSR = System mode, PC = `0x08000000`) and jumps straight to the cartridge.
In M3 we add HLE implementations of the common SWI calls (Div, Sqrt, CpuSet, LZ77/RL decompress, …)
so BIOS-less play works for most games; an open-source replacement BIOS is another option.

## 7. Build

- C++20, CMake ≥ 3.20 is the primary build (`cmake -B build && cmake --build build && ctest --test-dir build`).
- A plain `Makefile` mirrors it for machines without CMake (MinGW `make` / `mingw32-make`).
- Warnings on (`-Wall -Wextra -Wpedantic`), treated as errors in CI later.
- SDL2/SDL3 will be pulled via CMake `FetchContent` when the M5 frontend lands.

## 8. Code Style

- **Allman braces, always.** Every `{` goes on its own line directly under the statement that opens it
  (namespaces, classes, functions, control blocks, lambdas, even one-line getters), so it lines up with its `}`.
  `.clang-format` encodes this.
- **Explain things in sentences.** Every non-trivial function, member, type and tricky block gets a comment of
  at least one full sentence saying what it does and why. Hardware quirks always get a comment.
- **Banner header on every file**, naming what the file is. The `=` run matches the title's length so the
  closing `//` lines up; a short plain comment describing the file may follow:

  ```cpp
  // =============== //
  // CPU Interpreter //
  // =============== //

  // One or two sentences on what lives in this file.
  ```
- No `// namespace foo` comments after a namespace's closing brace.
- Naming: `PascalCase` types, `snake_case` functions and variables, trailing `_` for private members,
  `kPascalCase` constants.
- **Markdown headings capitalize every word**, small words included; text in backticks keeps its case.
- Each source folder keeps a `README.md` describing what's in it, updated alongside the code.

## 9. Known Simplifications (Current)

- Instruction cycle counts are approximate (internal cycles only, no N/S distinction, no wait states).
- Thumb state halts the CPU with an "unimplemented" error (M2).
- Without a BIOS, SWI and undefined-instruction exceptions halt with an error instead of jumping to an empty vector (HLE BIOS in M3).
- MUL/MLA leave the C flag unchanged (real hardware sets it to a meaningless value).
- Byte writes to VRAM/palette/OAM are stored as bytes (hardware duplicates/ignores them).
- Open-bus reads return 0; BIOS read protection not modelled.
- Only video mode 3 is rendered.
