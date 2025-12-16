#pragma once

#include <cstdint>
#include <array>

class MMU;

constexpr int SCREEN_WIDTH = 240;
constexpr int SCREEN_HEIGHT = 160;

class PPU {
public:
    PPU(MMU& mmu);

    void reset();
    void step(int cycles);

    bool isFrameReady() const { return frameReady; }
    void clearFrameReady() { frameReady = false; }

    const uint32_t* getFramebuffer() const { return framebuffer.data(); }

private:
    void renderScanline();
    void renderMode0();
    void renderMode3();
    void renderMode4();
    void renderMode5();
    void renderSprites();

    uint32_t rgb15to32(uint16_t color);

    MMU& mmu;

    int scanline = 0;
    int dot = 0;

    bool frameReady = false;

    std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> framebuffer{};
};
