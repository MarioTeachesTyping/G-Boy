# `src/core/cartridge/` — Game Pak Cartridge

A GBA game cartridge ("Game Pak") is mainly a ROM chip of up to 32 MB, plus optionally some save
memory. This folder holds the loaded ROM and the information parsed from its header.

## Files

| File | What's in it |
|---|---|
| [`cartridge.hpp`](cartridge.hpp) | The `Cartridge` class and the `CartridgeHeader` struct. |
| [`cartridge.cpp`](cartridge.cpp) | ROM loading, size checks, and header parsing. |

## Loading

`Cartridge::load()` takes the ROM bytes (read from disk by the frontend) and:

1. Rejects images smaller than the 192-byte header or larger than 32 MB, with a readable error.
2. Parses the header fields described below.
3. Keeps the bytes. The bus forwards reads in `0x08000000`–`0x0DFFFFFF` to `read8()`.

You normally don't call this directly. `Gba::load_rom()` calls it and then resets the console.

## The ROM Header

The first 192 bytes of every GBA ROM are a header the BIOS checks before booting:

| Offset | Field | Meaning |
|---|---|---|
| `0x00` | Entry point | A branch instruction to the game's real start. Execution begins here. |
| `0x04`–`0x9F` | Nintendo logo | Compressed logo bitmap; the real BIOS refuses to boot if it's wrong. Not checked by G-Boy. |
| `0xA0` | Title | Up to 12 upper-case ASCII characters. |
| `0xAC` | Game code | 4 characters identifying the release (e.g. `AXVE`). Useful later for per-game save-type quirks. |
| `0xB0` | Maker code | 2-character publisher code (`01` = Nintendo). |
| `0xB2` | Fixed value | Must be `0x96`. |
| `0xBC` | Version | Software revision. |
| `0xBD` | Complement checksum | `-(sum of bytes 0xA0..0xBC) - 0x19`, truncated to 8 bits. |

G-Boy records whether the checksum is valid (`checksum_valid`) but **still loads ROMs with a bad
one**, because lots of homebrew doesn't bother fixing it. The CLI prints a warning instead.

## What's Coming (M7)

Saves live on the cartridge too, in one of several chip types: SRAM, Flash 64K/128K, or EEPROM
512B/8K. Each is accessed differently, and games don't declare which one they use. Emulators detect
it by scanning the ROM for library ID strings such as `FLASH1M_V` or `EEPROM_V`. That detection and
the `.sav` file handling will be added here. For now the bus just exposes a plain 64 KB SRAM area.
