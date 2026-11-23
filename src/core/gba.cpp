// ======================= //
// Game Boy Advance System //
// ======================= //

// Component construction, reset / BIOS-skip logic, and the main run loop.

#include "core/gba.hpp"

#include <utility>

#include "core/memory/io_regs.hpp"

namespace gboy
{

Gba::Gba()
    : cart_(std::make_unique<Cartridge>()),
      bus_(std::make_unique<Bus>(*cart_)),
      cpu_(std::make_unique<Arm7tdmi>(*bus_)),
      ppu_(std::make_unique<Ppu>(*bus_))
{
    reset();
}

bool Gba::load_rom(std::vector<u8> rom, std::string& error)
{
    if (!cart_->load(std::move(rom), error))
    {
        return false;
    }
    reset();
    return true;
}

bool Gba::load_bios(const std::vector<u8>& bios, std::string& error)
{
    if (!bus_->load_bios(bios))
    {
        error = "BIOS must be exactly 16384 bytes (got " + std::to_string(bios.size()) + ")";
        return false;
    }
    has_bios_ = true;
    reset();
    return true;
}

void Gba::reset()
{
    cycles_ = 0;
    bus_->reset();

    // KEYINPUT is active-low, so 0x03FF means all ten buttons are released.
    bus_->io_set16(io::KEYINPUT, 0x03FF);

    cpu_->reset();
    cpu_->set_bios_loaded(has_bios_);
    ppu_->reset();
    if (!has_bios_)
    {
        skip_bios();
    }
}

void Gba::skip_bios()
{
    // These are the stack pointers the official BIOS sets up before jumping to the game.
    Arm7tdmi& cpu = *cpu_;
    cpu.set_banked_sp(CpuMode::Supervisor, 0x03007FE0);
    cpu.set_banked_sp(CpuMode::Irq, 0x03007FA0);
    cpu.set_cpsr(static_cast<u32>(CpuMode::System));
    cpu.set_reg(13, 0x03007F00);
    cpu.jump(0x08000000);
}

int Gba::step()
{
    const int cycles = cpu_->step();
    ppu_->tick(cycles);
    cycles_ += static_cast<u64>(cycles);
    return cycles;
}

void Gba::run_frame()
{
    const u64 target = ppu_->frame_count() + 1;
    while (ppu_->frame_count() < target && !cpu_->halted())
    {
        step();
    }
}

}
