// ================================== //
// System Tests (Cartridge, Bus, PPU) //
// ================================== //

// Checks ROM header parsing, memory-map mirroring and access rules, PPU
// scanline timing, and that a tiny program can put a pixel on screen.

#include "test_harness.hpp"

#include "core/memory/io_regs.hpp"

using namespace gboy;
using gboy::test::boot;

namespace
{

// Builds a 512-byte ROM with a valid header containing the given title and game code,
// including a correct complement checksum.
std::vector<u8> rom_with_header(const char* title, const char* code)
{
    std::vector<u8> rom(0x200, 0);
    for (int i = 0; title[i] && i < 12; ++i)
    {
        rom[0xA0 + i] = static_cast<u8>(title[i]);
    }
    for (int i = 0; code[i] && i < 4; ++i)
    {
        rom[0xAC + i] = static_cast<u8>(code[i]);
    }
    rom[0xB0] = '0';
    rom[0xB1] = '1';
    rom[0xB2] = 0x96;
    rom[0xBD] = Cartridge::compute_header_checksum(rom);
    return rom;
}

}

// Every header field should be read from its documented offset.
TEST(cartridge_parses_header)
{
    Cartridge cart;
    std::string error;
    CHECK(cart.load(rom_with_header("GBOYTEST", "GBOY"), error));
    CHECK(cart.header().title == "GBOYTEST");
    CHECK(cart.header().game_code == "GBOY");
    CHECK(cart.header().maker_code == "01");
    CHECK_EQ(cart.header().fixed_value, 0x96);
    CHECK(cart.header().checksum_valid);
}

// A corrupted checksum is still loadable (like many homebrew ROMs) but flagged as invalid.
TEST(cartridge_detects_bad_checksum)
{
    auto rom = rom_with_header("GBOYTEST", "GBOY");
    rom[0xBD] ^= 0xFF;
    Cartridge cart;
    std::string error;
    CHECK(cart.load(rom, error));
    CHECK(!cart.header().checksum_valid);
}

// A file too small to hold a header must be rejected with an error message.
TEST(cartridge_rejects_tiny_rom)
{
    Cartridge cart;
    std::string error;
    CHECK(!cart.load(std::vector<u8>(16, 0), error));
    CHECK(!error.empty());
}

// ROM writes are ignored, and the ROM is visible through all three wait-state mirrors.
TEST(bus_rom_is_read_only_and_mirrored)
{
    auto gba = boot({0x12345678});
    Bus& bus = gba->bus();
    bus.write32(0x08000000, 0);
    CHECK_EQ(bus.read32(0x08000000), 0x12345678u);
    CHECK_EQ(bus.read32(0x0A000000), 0x12345678u);
    CHECK_EQ(bus.read32(0x0C000000), 0x12345678u);
}

// IWRAM repeats every 32 KB, and VRAM's odd 96-in-128 KB mirroring maps 0x18000 back to 0x10000.
TEST(bus_iwram_and_vram_mirroring)
{
    auto gba = boot({});
    Bus& bus = gba->bus();
    bus.write16(0x03000010, 0xBEEF);
    CHECK_EQ(bus.read16(0x03008010), 0xBEEF);
    bus.write16(0x06010000, 0xCAFE);
    CHECK_EQ(bus.read16(0x06018000), 0xCAFE);
}

// The CPU must not be able to overwrite the read-only VCOUNT register.
TEST(bus_vcount_is_read_only)
{
    auto gba = boot({});
    gba->bus().write16(0x04000000 + io::VCOUNT, 0x55);
    CHECK_EQ(gba->bus().io_read16(io::VCOUNT), 0);
}

// HBlank starts after 960 cycles, lines advance every 1232, and VBlank begins at line 160.
TEST(ppu_scanline_timing)
{
    auto gba = boot({});
    Ppu& ppu = gba->ppu();
    ppu.tick(Ppu::kHDrawCycles);
    CHECK(gba->bus().io_read16(io::DISPSTAT) & 0x2);
    ppu.tick(Ppu::kCyclesPerLine - Ppu::kHDrawCycles);
    CHECK_EQ(ppu.line(), 1);
    CHECK_EQ(gba->bus().io_read16(io::VCOUNT), 1);
    ppu.tick(Ppu::kCyclesPerLine * 159);
    CHECK_EQ(ppu.line(), 160);
    CHECK(gba->bus().io_read16(io::DISPSTAT) & 0x1);
    CHECK_EQ(ppu.frame_count(), 1u);
}

// End-to-end: a program enables mode 3, writes one red pixel to VRAM, and it shows up
// in the framebuffer after a frame.
TEST(ppu_mode3_renders_pixel_written_by_program)
{
    auto gba = boot({
        0xE3A00404, // mov r0, #0x04000000
        0xE3A01B01, // mov r1, #0x400        (BG2 enable)
        0xE3811003, // orr r1, r1, #3        (mode 3)
        0xE5801000, // str r1, [r0]          (DISPCNT)
        0xE3A02406, // mov r2, #0x06000000
        0xE3A0301F, // mov r3, #0x1F         (red)
        0xE5823000, // str r3, [r2]          (pixel 0,0)
        0xEAFFFFFE, // b .
    });
    gba->run_frame();
    CHECK(!gba->cpu().halted());
    CHECK_EQ(gba->ppu().framebuffer()[0], 0x001F);
    CHECK_EQ(gba->ppu().framebuffer()[1], 0x0000);
}
