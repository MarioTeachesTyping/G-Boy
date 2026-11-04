// ===================== //
// Command-Line Frontend //
// ===================== //

// A headless runner for poking at the core without a window: prints the ROM
// header, runs N frames, reports CPU state, and can dump the screen to a PPM.
//
// Usage: gboy <rom.gba> [--bios gba_bios.bin] [--frames N] [--dump-ppm out.ppm]

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "core/gba.hpp"

namespace
{

// Reads an entire file into `out` as raw bytes. Returns false if it can't be opened.
bool read_file(const std::string& path, std::vector<gboy::u8>& out)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

// Writes the PPU's current framebuffer as a binary PPM image. PPM is used because it
// needs no image library and most image viewers can open it.
bool write_ppm(const std::string& path, const gboy::Ppu& ppu)
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        return false;
    }
    file << "P6\n" << gboy::Ppu::kWidth << ' ' << gboy::Ppu::kHeight << "\n255\n";

    // Scales a 5-bit colour channel to 8 bits, copying the top bits into the bottom
    // so that full intensity (31) becomes 255 rather than 248.
    const auto expand = [](unsigned c)
    {
        return static_cast<char>((c << 3) | (c >> 2));
    };

    for (gboy::u16 pixel : ppu.framebuffer())
    {
        // BGR555 stores red in the low bits, so unpack it into RGB order for PPM.
        const char rgb[3] = {expand(pixel & 0x1F), expand((pixel >> 5) & 0x1F), expand((pixel >> 10) & 0x1F)};
        file.write(rgb, 3);
    }
    return static_cast<bool>(file);
}

// Prints the command-line syntax to stderr.
void print_usage()
{
    std::fprintf(stderr, "usage: gboy <rom.gba> [--bios <bios.bin>] [--frames <n>] [--dump-ppm <out.ppm>]\n");
}

// Prints all 16 registers, CPSR and the next PC in a compact table.
void print_registers(gboy::Arm7tdmi& cpu)
{
    for (unsigned i = 0; i < 16; ++i)
    {
        std::printf("  r%-2u = 0x%08X%s", i, cpu.reg(i), (i % 4 == 3) ? "\n" : "");
    }
    std::printf("  cpsr = 0x%08X  pc(next) = 0x%08X  %s\n", cpu.cpsr(), cpu.pc(), cpu.thumb() ? "THUMB" : "ARM");
}

}

int main(int argc, char** argv)
{
    std::string rom_path;
    std::string bios_path;
    std::string ppm_path;
    long frames = 1;

    // Parse arguments: the first non-flag argument is the ROM, everything else is a flag.
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--bios" && has_value)
        {
            bios_path = argv[++i];
        }
        else if (arg == "--frames" && has_value)
        {
            frames = std::strtol(argv[++i], nullptr, 10);
        }
        else if (arg == "--dump-ppm" && has_value)
        {
            ppm_path = argv[++i];
        }
        else if (arg == "-h" || arg == "--help")
        {
            print_usage();
            return 0;
        }
        else if (rom_path.empty() && arg[0] != '-')
        {
            rom_path = arg;
        }
        else
        {
            print_usage();
            return 1;
        }
    }
    if (rom_path.empty())
    {
        print_usage();
        return 1;
    }

    gboy::Gba gba;
    std::string error;

    // The BIOS is optional; without it the console boots directly into the cartridge.
    if (!bios_path.empty())
    {
        std::vector<gboy::u8> bios;
        if (!read_file(bios_path, bios) || !gba.load_bios(bios, error))
        {
            std::fprintf(stderr, "failed to load BIOS '%s': %s\n", bios_path.c_str(), error.c_str());
            return 1;
        }
    }

    std::vector<gboy::u8> rom;
    if (!read_file(rom_path, rom))
    {
        std::fprintf(stderr, "could not read ROM '%s'\n", rom_path.c_str());
        return 1;
    }
    if (!gba.load_rom(std::move(rom), error))
    {
        std::fprintf(stderr, "failed to load ROM: %s\n", error.c_str());
        return 1;
    }

    const auto& h = gba.cartridge().header();
    std::printf("Title:     %s\n", h.title.c_str());
    std::printf("Game code: %s  Maker: %s  Version: %u\n", h.game_code.c_str(), h.maker_code.c_str(), h.version);
    std::printf("Header:    fixed=0x%02X%s  checksum=0x%02X (%s)\n", h.fixed_value,
                h.fixed_value == 0x96 ? "" : " (expected 0x96)", h.checksum, h.checksum_valid ? "ok" : "BAD");
    std::printf("Boot:      %s\n", gba.has_bios() ? "BIOS" : "direct to cartridge (no BIOS)");

    for (long f = 0; f < frames && !gba.cpu().halted(); ++f)
    {
        gba.run_frame();
    }

    std::printf("\nRan %llu frame(s), %llu cycles.\n", static_cast<unsigned long long>(gba.ppu().frame_count()),
                static_cast<unsigned long long>(gba.cycles()));
    if (gba.cpu().halted())
    {
        std::printf("CPU stopped: %s\n", gba.cpu().error().c_str());
    }
    print_registers(gba.cpu());

    if (!ppm_path.empty())
    {
        if (!write_ppm(ppm_path, gba.ppu()))
        {
            std::fprintf(stderr, "could not write '%s'\n", ppm_path.c_str());
            return 1;
        }
        std::printf("Wrote framebuffer to %s\n", ppm_path.c_str());
    }
    return 0;
}
