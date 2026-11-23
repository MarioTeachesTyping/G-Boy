# `src/frontend/cli/` — Command-Line Frontend

`gboy` is a headless runner: no window, no input, no sound. It exists so we can poke at the core
from a terminal while the real frontend (M5) doesn't exist yet. It's also handy for automated
test-ROM checks.

## Files

| File | What's in it |
|---|---|
| [`main.cpp`](main.cpp) | Argument parsing, file loading, the run loop, the register dump, and the PPM image writer. |

## Usage

```bash
build/gboy path/to/game.gba [--bios gba_bios.bin] [--frames N] [--dump-ppm out.ppm]
```

| Option | Default | Effect |
|---|---|---|
| `--bios <file>` | none | Boot through a real 16 KB BIOS instead of jumping straight into the game. |
| `--frames <n>` | `1` | Number of frames to run (60 ≈ one second of game time). Stops early if the CPU halts. |
| `--dump-ppm <file>` | none | After running, save the last frame as a PPM image (opens in most image viewers, GIMP, IrfanView, ...). |

## What It Prints

```
Title:     GBA Tests
Game code: 1337  Maker: JS  Version: 0
Header:    fixed=0x96  checksum=0x69 (ok)
Boot:      direct to cartridge (no BIOS)

Ran 120 frame(s), 33623745 cycles.
  r0  = 0x030048C0  r1  = 0x08001D24  ...
  ...
  cpsr = 0x6000001F  pc(next) = 0x08001EC4  ARM
```

1. **Header info.** The title, game code and maker code, plus whether the header's fixed byte and
   checksum are valid.
2. **Run summary.** Frames completed and total CPU cycles.
3. **`CPU stopped: ...`**, printed only if the emulator halted. This line is the most useful output
   right now: it names exactly what's missing and where, e.g.
   `Thumb state not implemented yet (M2) at 0x08000290`.
4. **Register dump.** All 16 registers, CPSR, the next instruction address, and the ARM/Thumb state.

## Running Test ROMs

Many test ROMs report results in a register, so the register dump is enough to read them without a
working display. For example, jsmolka's `arm.gba` leaves the number of the first failing test in
`r12` (0 = all passed):

```bash
build/gboy roms/test/arm.gba --frames 120
```

Put test ROMs in `roms/test/`, which git ignores.
