// ================== //
// Game Pak Cartridge //
// ================== //

// Holds the loaded ROM image and the information parsed from its header.
// Save memory (SRAM / Flash / EEPROM) detection will live here in M7.

#pragma once

#include <string>
#include <vector>

#include "core/common.hpp"

namespace gboy
{

// The fields we care about from the 192-byte header at the start of every GBA ROM.
struct CartridgeHeader
{
    // Game title from offset 0xA0, up to 12 upper-case ASCII characters.
    std::string title;

    // Four-character game code from offset 0xAC (e.g. "AXVE"), unique per release.
    std::string game_code;

    // Two-character publisher code from offset 0xB0 ("01" is Nintendo).
    std::string maker_code;

    // Byte at 0xB2; must be 0x96 on a valid cartridge.
    u8 fixed_value = 0;

    // Software version number from offset 0xBC.
    u8 version = 0;

    // Header complement checksum from 0xBD; the real BIOS refuses to boot if it is wrong.
    u8 checksum = 0;

    // True when `checksum` matches the value we compute from the header bytes.
    bool checksum_valid = false;
};

// A loaded Game Pak. The bus forwards reads in 0x08000000-0x0DFFFFFF to this class.
class Cartridge
{
public:
    // GBA cartridges address at most 32 MB of ROM.
    static constexpr std::size_t kMaxRomSize = 32 * 1024 * 1024;

    // Anything shorter than the header cannot be a real ROM.
    static constexpr std::size_t kHeaderSize = 0xC0;

    Cartridge() = default;

    // Takes ownership of a ROM image and parses its header. Returns false and fills
    // `error` with a readable reason if the image is too small or too large.
    bool load(std::vector<u8> rom, std::string& error);

    // Returns the parsed header of the currently loaded ROM.
    const CartridgeHeader& header() const
    {
        return header_;
    }

    // Returns the raw ROM bytes.
    const std::vector<u8>& rom() const
    {
        return rom_;
    }

    // Returns whether a ROM has been loaded.
    bool loaded() const
    {
        return !rom_.empty();
    }

    // Reads one ROM byte. `offset` is relative to the start of ROM (the 0x08000000 base
    // already removed). Reads past the end return 0 until open-bus is modelled in M3.
    u8 read8(u32 offset) const
    {
        return offset < rom_.size() ? rom_[offset] : 0;
    }

    // Computes the header checksum the same way the BIOS does:
    // -(sum of bytes 0xA0..0xBC) - 0x19, truncated to 8 bits.
    static u8 compute_header_checksum(const std::vector<u8>& rom);

private:
    // The full ROM image exactly as loaded from disk.
    std::vector<u8> rom_;

    // Header fields parsed out of `rom_` at load time.
    CartridgeHeader header_;
};

}
