#pragma once

#include <cstdint>
#include <array>

class MMU;

class Timer {
public:
    Timer(MMU& mmu);
    
    void reset();
    void step(int cycles);
    
    uint16_t readCounter(int timer) const;
    uint16_t readControl(int timer) const;
    
    void writeReload(int timer, uint16_t value);
    void writeControl(int timer, uint16_t value);
    
private:
    void tick(int timer);
    void overflow(int timer);
    
    MMU& mmu;
    
    std::array<uint16_t, 4> counter{};
    std::array<uint16_t, 4> reload{};
    std::array<uint16_t, 4> control{};
    std::array<int, 4> prescalerCounter{};
    
    static constexpr int prescalerShifts[4] = {0, 6, 8, 10};
};
