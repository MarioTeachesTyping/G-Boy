# `src/` — Source Code

Everything that gets compiled into G-Boy lives here, split into two halves on purpose:

| Folder | What it is | Depends on |
|---|---|---|
| [`core/`](core/) | The emulator itself, built as the `gboy_core` static library. CPU, memory, graphics, cartridge. | Only the C++ standard library |
| [`frontend/`](frontend/) | Programs that *use* the core: today a command-line runner, later an SDL window and the debugger UI. | `core/` plus whatever platform library the frontend needs |

## Why The Split Matters

The core never opens files, creates windows, plays audio, or reads the keyboard. It only
works with byte buffers and plain function calls: you hand it a ROM as a `std::vector<u8>`,
call `run_frame()`, and read the finished image from the PPU's framebuffer.

That gives us:

- **Testability.** The unit tests in [`tests/`](../tests/) link straight against `gboy_core` and drive it
  instruction by instruction, with no window or real ROM needed.
- **Multiple frontends for free.** The CLI, the planned SDL frontend (M5), and the planned
  debugger (M8) all talk to the same `gboy::Gba` object. Nothing in the core has to change to add one.
- **Portability.** Platform-specific code is confined to `frontend/`, so porting means writing a new
  frontend, not touching the emulator.

## Include Paths

`src/` is the include root (`-Isrc`), so every include is written from here:

```cpp
#include "core/gba.hpp"
#include "core/cpu/arm7tdmi.hpp"
```

## Code Style

All C++ here follows the rules in [docs/SPEC.md §8](../docs/SPEC.md): Allman braces, a sentence-or-more
comment on anything non-trivial, and a boxed title banner at the top of every file.
