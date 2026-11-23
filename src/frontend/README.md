# `src/frontend/` — Frontends

A **frontend** is a program that wraps the emulator core and connects it to the outside world:
files, screens, keyboards, speakers. The core ([`../core/`](../core/)) knows nothing about any of
that. Each frontend is a separate executable built on `gboy_core`.

| Folder | Status | What it is |
|---|---|---|
| [`cli/`](cli/) | ✅ Working | `gboy`, a headless command-line runner for loading ROMs, running frames, and inspecting CPU state. |
| `sdl/` | Planned (M5) | A real window with scaling, keyboard/gamepad input, frame pacing and audio output. |
| `debugger/` | Planned (M8) | The "twist": a visual CPU debugger with a disassembler, breakpoints, a register view and a memory viewer. |

## What Every Frontend Does

They all follow the same pattern:

1. Read the ROM (and optionally a BIOS) from disk into a byte vector.
2. Hand them to a `gboy::Gba` with `load_rom()` / `load_bios()`.
3. Call `run_frame()` in a loop.
4. Do something with `gba.ppu().framebuffer()`: display it, save it, compare it.
5. Check `gba.cpu().halted()` and show `gba.cpu().error()` if the emulator stopped.

Later frontends will also write the button state into `KEYINPUT` and drain audio samples.
