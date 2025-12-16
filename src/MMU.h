#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>

class PPU;

class MMU {
public:
    MMU();

    void connectPPU(PPU* ppu) { this->ppu = ppu; }

    bool loadROM(const std::string& path);
    void reset();

    uint8_t read8(uint32_t address);
    uint16_t read16(uint32_t address);
    uint32_t read32(uint32_t address);

    void write8(uint32_t address, uint8_t value);
    void write16(uint32_t address, uint16_t value);
    void write32(uint32_t address, uint32_t value);

    uint16_t readIO(uint32_t address);
    void writeIO(uint32_t address, uint16_t value);

    uint8_t* getVRAM() { return vram.data(); }
    uint8_t* getPalette() { return palette.data(); }
    uint8_t* getOAM() { return oam.data(); }

    uint16_t getDisplayControl() const { return io[0]; }
    uint16_t getDisplayStatus() const { return io[2]; }
    uint16_t getVCount() const { return io[3]; }

    void setVCount(uint16_t value) { io[3] = value; }
    void setDisplayStatus(uint16_t value) { io[2] = value; }

    uint16_t getBGControl(int bg) const { return io[0x08 / 2 + bg]; }
    uint16_t getBGXOffset(int bg) const { return io[0x10 / 2 + bg * 2]; }
    uint16_t getBGYOffset(int bg) const { return io[0x12 / 2 + bg * 2]; }

    uint16_t getIE() const { return io[0x100]; }
    uint16_t getIF() const { return io[0x101]; }
    void setIF(uint16_t value) { io[0x101] = value; }
    uint16_t getIME() const { return io[0x104]; }

    void setKeyInput(uint16_t state) { keyInput = state; }

private:
    std::array<uint8_t, 0x4000> bios{};
    std::array<uint8_t, 0x40000> ewram{};
    std::array<uint8_t, 0x8000> iwram{};
    std::array<uint16_t, 0x200> io{};
    std::array<uint8_t, 0x400> palette{};
    std::array<uint8_t, 0x18000> vram{};
    std::array<uint8_t, 0x400> oam{};
    std::vector<uint8_t> rom;
    std::array<uint8_t, 0x10000> sram{};

    PPU* ppu = nullptr;

    bool biosLoaded = false;
    uint16_t keyInput = 0x03FF;
};
