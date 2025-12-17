#pragma once

#include <cstdint>
#include <array>

class MMU;

enum class DMAStartTiming {
    Immediate = 0,
    VBlank = 1,
    HBlank = 2,
    Special = 3
};

class DMA {
public:
    DMA(MMU& mmu);
    
    void reset();
    
    void checkImmediate(int channel);
    void triggerVBlank();
    void triggerHBlank();
    
    uint32_t readSource(int channel) const;
    uint32_t readDest(int channel) const;
    uint16_t readCount(int channel) const;
    uint16_t readControl(int channel) const;
    
    void writeSource(int channel, uint32_t value, bool high);
    void writeDest(int channel, uint32_t value, bool high);
    void writeCount(int channel, uint16_t value);
    void writeControl(int channel, uint16_t value);
    
private:
    void execute(int channel);
    
    MMU& mmu;
    
    std::array<uint32_t, 4> source{};
    std::array<uint32_t, 4> dest{};
    std::array<uint32_t, 4> internalSource{};
    std::array<uint32_t, 4> internalDest{};
    std::array<uint16_t, 4> count{};
    std::array<uint16_t, 4> control{};
};
