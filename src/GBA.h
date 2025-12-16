#pragma once

#include <string>
#include <memory>

class CPU;
class MMU;
class PPU;

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

private:
    std::unique_ptr<MMU> mmu;
    std::unique_ptr<CPU> cpu;
    std::unique_ptr<PPU> ppu;
};
