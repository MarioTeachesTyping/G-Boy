# `tests/` — Unit Tests

These tests check the emulator core piece by piece, without real games. They build into a single
executable, `gboy_tests`, that links against `gboy_core`.

```bash
make test                                   # MinGW / make
ctest --test-dir build --output-on-failure  # CMake
```

Each test prints `[ OK ]` or `[FAIL]`, failed checks show the file, line and actual vs expected
values in hex, and the program exits non-zero if anything failed.

## Files

| File | What it tests |
|---|---|
| [`test_harness.hpp`](test_harness.hpp) | Not tests: the tiny framework itself (`TEST`, `CHECK`, `CHECK_EQ`) plus the `boot()` / `make_rom()` / `run_steps()` helpers. |
| [`main.cpp`](main.cpp) | The runner: executes every registered test and prints the summary. |
| [`test_cpu_arm.cpp`](test_cpu_arm.cpp) | Core ARM behaviour: boot state, ALU ops and flags, barrel shifter, branches, LDR/STR, MSR/MRS mode switching, register banking, the Thumb handoff, and the Rd = 15 compare quirk. |
| [`test_cpu_arm_multiply.cpp`](test_cpu_arm_multiply.cpp) | `MUL`, `MLA` and the four 64-bit long multiplies, signed vs unsigned, accumulate, flags. |
| [`test_cpu_arm_transfer.cpp`](test_cpu_arm_transfer.cpp) | Halfword/signed loads and stores, misaligned-load quirks, `SWP`, and `LDM`/`STM` with every quirk (base in list, empty list, `^` user bank, exception return). |
| [`test_cpu_arm_exceptions.cpp`](test_cpu_arm_exceptions.cpp) | `SWI` and undefined-instruction traps, both with a tiny fake BIOS and without any BIOS, plus `SUBS pc, lr, #4` IRQ-style returns. |
| [`test_system.cpp`](test_system.cpp) | Cartridge header parsing, bus mirroring and read-only rules, PPU scanline timing, and an end-to-end "draw a pixel in mode 3" program. |

## How A CPU Test Works

There's no assembler in the build, so tests use **hand-assembled ARM opcodes**, with the assembly
written in a comment next to each one:

```cpp
// MUL multiplies two registers into a third.
TEST(mul_basic)
{
    auto gba = boot({
        0xE3A00006, // mov r0, #6
        0xE3A01007, // mov r1, #7
        0xE0020190, // mul r2, r0, r1
    });
    run_steps(*gba, 3);
    CHECK_EQ(gba->cpu().reg(2), 42u);
}
```

- `boot({...})` turns the opcode list into a small ROM, loads it, and boots without a BIOS, so the
  first opcode runs at `0x08000000` in System mode.
- `run_steps(gba, n)` executes exactly `n` instructions.
- You can also set up state directly, e.g. `gba->cpu().set_reg(...)`, `gba->bus().write32(...)`,
  `gba->cpu().set_cpsr(...)`, instead of spending instructions on it.

## Adding A Test

1. Pick the file for the area you're testing, or make a new `test_<area>.cpp` and add it to the test
   list in `CMakeLists.txt`. The Makefile picks up new files automatically.
2. Write a one-sentence comment above the test saying what behaviour it pins down.
3. `TEST(descriptive_snake_case_name)` + Allman braces, like everything else.

`CHECK` keeps going after a failure, so one test can report several broken things at once.

## Test ROMs

Unit tests prove individual instructions. **Test ROMs** prove whole subsystems against real hardware
behaviour. They aren't committed. Download them into `roms/test/` (git-ignored) and run them through
the [CLI](../src/frontend/cli/). The plan for which ROMs gate which milestone is in
[docs/SPEC.md §5](../docs/SPEC.md).
