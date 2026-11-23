# `src/core/cpu/` — CPU Interpreter (ARM7TDMI)

The GBA's processor is an **ARM7TDMI** running at 16.78 MHz (2²⁴ Hz). It implements the ARMv4T
architecture, which has two instruction sets:

- **ARM**: 32-bit instructions with lots of features. Every instruction can be conditional and can
  use the barrel shifter.
- **Thumb**: 16-bit compressed instructions. Most game code is Thumb, because the cartridge bus is
  only 16 bits wide. *(Not implemented yet; this is milestone M2.)*

## Files

| File | What's in it |
|---|---|
| [`arm7tdmi.hpp`](arm7tdmi.hpp) | The `Arm7tdmi` class, the `CpuMode` enum, and the `psr::` flag masks. This is the public face of the CPU. |
| [`arm7tdmi.cpp`](arm7tdmi.cpp) | Everything that isn't a specific instruction: reset, mode switching and register banking, condition codes, exception entry, and the `step()` fetch/execute loop. |
| [`arm_isa.cpp`](arm_isa.cpp) | The decoder and a handler for every 32-bit ARM instruction. |

## Registers And Modes

The CPU has 16 visible registers, `r0`–`r15`. Three of them have special jobs:

| Register | Role |
|---|---|
| `r13` (SP) | Stack pointer, by convention |
| `r14` (LR) | Link register: `BL` stores the return address here |
| `r15` (PC) | Program counter |

There are also two status registers. **CPSR** holds the N/Z/C/V flags, the IRQ/FIQ disable bits,
the Thumb bit, and the current mode. Each exception mode also has an **SPSR**, a saved copy of
CPSR from when the exception happened.

The CPU runs in one of seven **modes** (User, System, IRQ, FIQ, Supervisor, Abort, Undefined).
Switching mode swaps in that mode's own private ("banked") copies of some registers:

- Every exception mode has its own `r13` and `r14`, so an interrupt handler gets its own stack and
  doesn't destroy the interrupted code's return address.
- FIQ mode additionally has its own `r8`–`r12`.
- User and System share one bank. Games mostly run in System mode.

`switch_mode()` in `arm7tdmi.cpp` handles this by parking the outgoing registers in storage arrays
and loading the incoming ones. `set_cpsr()` calls it automatically whenever the mode bits change.

## The Pipeline Trick: Why `r15` Is "+8"

The real ARM7TDMI fetches two instructions ahead of the one it's executing. So when an instruction
reads `r15`, it sees **its own address + 8** in ARM state (+4 in Thumb). Code like `ldr r0, [pc, #4]`
depends on that.

We don't simulate a real pipeline. Instead `r15` is **always stored in its "+8" form**:

- `step()` fetches the instruction at `r15 - 8`, executes it, then adds 4.
- Anything that writes `r15` (branches, `mov pc, ...`, `ldm {..., pc}`) calls `flush_pipeline()`,
  which aligns the new address and re-adds the +8. It also sets a flag so `step()` doesn't add 4 on top.
- `pc()` undoes the offset and returns "the address of the next instruction", which is what humans
  (and the debugger) want to see.

A few instructions see `r15` one word further ahead (+12) because they spend an extra internal
cycle: register-specified shifts, and `STR`/`STM` storing `r15`. Each of those is commented in the code.

## Instruction Decoding

`execute_arm()` first checks the 4-bit condition code (the top bits of every ARM instruction). If
the condition fails, the instruction is skipped. Otherwise it matches the opcode against a list of
bit patterns, **ordered from most to least specific**. Several instruction classes hide inside
the broad "data processing" encoding (e.g. `MUL`, `SWP` and `MRS` all look like data processing
with unusual bits), so they have to be caught first.

What's implemented:

| Group | Instructions |
|---|---|
| Branches | `B`, `BL`, `BX` (including switching to Thumb) |
| Data processing | All 16: `AND EOR SUB RSB ADD ADC SBC RSC TST TEQ CMP CMN ORR MOV BIC MVN`, with every barrel-shifter form |
| PSR transfer | `MRS`, `MSR` (with field masks; User mode can only change flags) |
| Multiply | `MUL`, `MLA`, `UMULL`, `UMLAL`, `SMULL`, `SMLAL` |
| Single transfer | `LDR`, `STR`, `LDRB`, `STRB` |
| Halfword / signed | `LDRH`, `STRH`, `LDRSB`, `LDRSH` |
| Block transfer | `LDM`, `STM`, all four addressing modes (IA/IB/DA/DB) |
| Swap | `SWP`, `SWPB` |
| Exceptions | `SWI`, the undefined-instruction trap (coprocessor instructions count as undefined, since the GBA has no coprocessors) |

## Hardware Quirks We Reproduce

Real games and test ROMs depend on these, so they're emulated on purpose. Each one is commented
where it happens:

- **Barrel shifter edge cases.** `LSR #0` means `LSR #32`, `ASR #0` means `ASR #32`, `ROR #0` means
  `RRX`, and shifts by 32 or more have their own carry rules.
- **Unaligned loads rotate.** `LDR` from a misaligned address reads the aligned word and rotates it.
  `LDRH` rotates the halfword, and a misaligned `LDRSH` turns into `LDRSB`.
- **LDM/STM with the base register in the list.** `STM` stores the *old* base if it's the first
  register and the *new* base otherwise. On `LDM` the loaded value beats the write-back.
- **Empty register list.** It transfers only `r15`, but moves the base by 0x40 as if all 16
  registers had been transferred.
- **The `^` suffix.** `LDM {..., pc}^` returns from an exception (CPSR ← SPSR). Any other `^`
  transfer uses the User-mode registers.
- **`S` with destination `r15`.** `MOVS pc, lr` / `SUBS pc, lr, #4` restore CPSR from SPSR, and so
  does a compare instruction with Rd = 15, which *doesn't* branch. This one was found by jsmolka's `arm.gba`.

## Exceptions

`raise_exception()` performs the hardware's exception entry. It saves CPSR into the new mode's SPSR,
switches mode, stores the return address in that mode's `r14`, switches to ARM state with IRQs off,
and jumps to the vector address (`0x04` undefined, `0x08` SWI; IRQ at `0x18` arrives in M3).

The vectors are inside the BIOS. When no BIOS is loaded, `SWI` and undefined instructions **halt the
CPU with an explanatory error** instead of jumping into empty memory. `SWI` reports which BIOS
function was asked for (e.g. `SWI 0x06` = Div). M3 will add high-level emulation of those functions.

## Cycle Counts

Every handler returns a cycle count, which the PPU uses to keep time. They're approximations for now
(they count internal cycles but treat every memory access as one cycle). Accurate N/S cycles and
cartridge wait states arrive in M3.

## Validation

- Unit tests in [`tests/test_cpu_arm*.cpp`](../../../tests/) cover every instruction group and every quirk above.
- jsmolka's `arm.gba` test ROM passes all of its ARM tests. Test #51 is skipped because it uses Thumb.
