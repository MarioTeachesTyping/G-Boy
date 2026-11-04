// ================== //
// Game Pak Cartridge //
// ================== //

// ROM loading, size validation, and cartridge header parsing.

#include "core/cartridge/cartridge.hpp"

#include <utility>

namespace gboy
{

namespace
{

// Reads a fixed-length ASCII field from the header, stopping early at a NUL byte
// because shorter titles are padded with zeros.
std::string read_ascii(const std::vector<u8>& rom, std::size_t offset, std::size_t length)
{
    std::string out;
    for (std::size_t i = 0; i < length; ++i)
    {
        const char c = static_cast<char>(rom[offset + i]);
        if (c == '\0')
        {
            break;
        }
        out.push_back(c);
    }
    return out;
}

}

u8 Cartridge::compute_header_checksum(const std::vector<u8>& rom)
{
    u8 sum = 0;
    for (std::size_t i = 0xA0; i <= 0xBC; ++i)
    {
        sum = static_cast<u8>(sum - rom[i]);
    }
    return static_cast<u8>(sum - 0x19);
}

bool Cartridge::load(std::vector<u8> rom, std::string& error)
{
    if (rom.size() < kHeaderSize)
    {
        error = "ROM is too small to contain a header (" + std::to_string(rom.size()) + " bytes)";
        return false;
    }
    if (rom.size() > kMaxRomSize)
    {
        error = "ROM is larger than 32 MB";
        return false;
    }

    header_ = {};
    header_.title = read_ascii(rom, 0xA0, 12);
    header_.game_code = read_ascii(rom, 0xAC, 4);
    header_.maker_code = read_ascii(rom, 0xB0, 2);
    header_.fixed_value = rom[0xB2];
    header_.version = rom[0xBC];
    header_.checksum = rom[0xBD];
    header_.checksum_valid = compute_header_checksum(rom) == header_.checksum;

    rom_ = std::move(rom);
    return true;
}

}
