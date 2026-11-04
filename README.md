# G-Boy

A Game Boy Advance emulator written in C++20.

Status: early scaffold (milestone M0). See [docs/SPEC.md](docs/SPEC.md) for the full spec and roadmap.

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
