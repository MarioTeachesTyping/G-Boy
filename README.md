# G-Boy

A Game Boy Advance emulator written in C++20.

Status: the ARM CPU is complete (milestone M1; passes jsmolka's `arm.gba` apart from its one Thumb test).
Thumb (M2) is next. See [docs/SPEC.md](docs/SPEC.md) for the full spec and roadmap.

## Project Layout

Each folder has its own README explaining what's in it:

| Folder | Contents |
|---|---|
| [`src/core/`](src/core/) | The emulator library: [CPU](src/core/cpu/), [memory bus](src/core/memory/), [cartridge](src/core/cartridge/), [graphics](src/core/ppu/) |
| [`src/frontend/`](src/frontend/) | Programs built on the core; currently the [command-line runner](src/frontend/cli/) |
| [`tests/`](tests/) | Unit tests |
| [`docs/`](docs/) | The spec and roadmap |

## Build

With CMake (preferred):

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Without CMake (MinGW / any `make` + `g++`):

```bash
make test
```

## Run

```bash
build/gboy path/to/game.gba --frames 60 --dump-ppm frame.ppm
```

Options: `--bios <gba_bios.bin>` (optional; without it the emulator boots straight into the cartridge),
`--frames <n>`, `--dump-ppm <file>` (writes the last frame as an image).

ROMs and BIOS dumps are not included and are git-ignored.
