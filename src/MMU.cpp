#include "MMU.h"
#include "PPU.h"
#include "Utils.h"
#include <fstream>

MMU::MMU() {
    reset();
}

void MMU::reset() {
    bios.fill(0);
    ewram.fill(0);
    iwram.fill(0);
    io.fill(0);
    palette.fill(0);
    vram.fill(0);
    oam.fill(0);
    sram.fill(0);
}

bool MMU::loadROM(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    rom.resize(size);
    if (!file.read(reinterpret_cast<char*>(rom.data()), size)) {
        return false;
    }

    return true;
}

uint8_t MMU::read8(uint32_t address) {
    uint32_t region = (address >> 24) & 0xFF;

    switch (region) {
        case 0x00:
            if (address < 0x4000 && biosLoaded) {
                return bios[address];
            }
            return 0;
        case 0x02:
            return ewram[address & 0x3FFFF];
        case 0x03:
            return iwram[address & 0x7FFF];
        case 0x04: {
            uint32_t reg = (address & 0x3FF) >> 1;
            if (address & 1) {
                return (io[reg] >> 8) & 0xFF;
            }
            return io[reg] & 0xFF;
        }
        case 0x05:
            return palette[address & 0x3FF];
        case 0x06:
            address &= 0x1FFFF;
            if (address >= 0x18000) {
                address -= 0x8000;
            }
            return vram[address];
        case 0x07:
            return oam[address & 0x3FF];
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
            address &= 0x01FFFFFF;
            if (address < rom.size()) {
                return rom[address];
            }
            return 0;
        case 0x0E:
        case 0x0F:
            return sram[address & 0xFFFF];
    }

    return 0;
}

uint16_t MMU::read16(uint32_t address) {
    address &= ~1;
    return read8(address) | (read8(address + 1) << 8);
}

uint32_t MMU::read32(uint32_t address) {
    address &= ~3;
    return read8(address) | (read8(address + 1) << 8) |
           (read8(address + 2) << 16) | (read8(address + 3) << 24);
}

void MMU::write8(uint32_t address, uint8_t value) {
    uint32_t region = (address >> 24) & 0xFF;

    switch (region) {
        case 0x02:
            ewram[address & 0x3FFFF] = value;
            break;
        case 0x03:
            iwram[address & 0x7FFF] = value;
            break;
        case 0x04: {
            uint32_t reg = (address & 0x3FF) >> 1;
            if (address & 1) {
                io[reg] = (io[reg] & 0x00FF) | (value << 8);
            } else {
                io[reg] = (io[reg] & 0xFF00) | value;
            }
            break;
        }
        case 0x05:
            palette[address & 0x3FF] = value;
            break;
        case 0x06: {
            address &= 0x1FFFF;
            if (address >= 0x18000) address -= 0x8000;
            uint32_t base = address & ~1;
            vram[base] = value;
            vram[base + 1] = value;
            break;
        }
        case 0x07:
            oam[address & 0x3FF] = value;
            break;
        case 0x0E:
        case 0x0F:
            sram[address & 0xFFFF] = value;
            break;
    }
}

void MMU::write16(uint32_t address, uint16_t value) {
    uint32_t region = (address >> 24) & 0xFF;
    if (region == 0x06) {
        address &= 0x1FFFF;
        if (address >= 0x18000) address -= 0x8000;
        address &= ~1;  
        vram[address] = value & 0xFF;
        vram[address + 1] = (value >> 8) & 0xFF;
        return;
    }

    address &= ~1;
    write8(address, value & 0xFF);
    write8(address + 1, (value >> 8) & 0xFF);
}

void MMU::write32(uint32_t address, uint32_t value) {
    uint32_t region = (address >> 24) & 0xFF;
    if (region == 0x06) {
        write16(address, value & 0xFFFF);
        write16(address + 2, (value >> 16) & 0xFFFF);
        return;
    }

    address &= ~3;
    write8(address, value & 0xFF);
    write8(address + 1, (value >> 8) & 0xFF);
    write8(address + 2, (value >> 16) & 0xFF);
    write8(address + 3, (value >> 24) & 0xFF);
}

uint16_t MMU::readIO(uint32_t address) {
    uint32_t reg = (address & 0x3FF) >> 1;
    return io[reg];
}

void MMU::writeIO(uint32_t address, uint16_t value) {
    uint32_t reg = (address & 0x3FF) >> 1;
    io[reg] = value;
}
