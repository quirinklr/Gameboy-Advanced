#pragma once

#include <string>
#include <memory>

class CPU;
class MMU;
class PPU;
class Timer;
class DMA;
class APU;

class GBA {
public:
    GBA();
    ~GBA();

    bool loadROM(const std::string& path);
    void reset();
    void runFrame();

    const uint32_t* getFramebuffer() const;
    bool isFrameReady() const;
    void clearFrameReady();

    void updateKey(int id, bool pressed);
    
    uint16_t getDISPCNT() const;
    uint16_t getIME() const;
    uint16_t getIE() const;

private:
    std::unique_ptr<MMU> mmu;
    std::unique_ptr<CPU> cpu;
    std::unique_ptr<PPU> ppu;
    std::unique_ptr<Timer> timer;
    std::unique_ptr<DMA> dma;
    std::unique_ptr<APU> apu;
};
