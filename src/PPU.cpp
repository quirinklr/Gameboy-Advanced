#include "PPU.h"
#include "MMU.h"

PPU::PPU(MMU& mmu) : mmu(mmu) {
    reset();
}

void PPU::reset() {
    scanline = 0;
    dot = 0;
    frameReady = false;
    framebuffer.fill(0xFF000000);
}

void PPU::step(int cycles) {
    dot += cycles;

    constexpr int HDRAW_CYCLES = 960;
    constexpr int HBLANK_CYCLES = 272;
    constexpr int SCANLINE_CYCLES = HDRAW_CYCLES + HBLANK_CYCLES;
    constexpr int VDRAW_LINES = 160;
    constexpr int VBLANK_LINES = 68;
    constexpr int TOTAL_LINES = VDRAW_LINES + VBLANK_LINES;

    while (dot >= SCANLINE_CYCLES) {
        dot -= SCANLINE_CYCLES;

        if (scanline < VDRAW_LINES) {
            renderScanline();
        }

        scanline++;

        if (scanline >= TOTAL_LINES) {
            scanline = 0;
            frameReady = true;
        }

        mmu.setVCount(scanline);

        uint16_t dispstat = mmu.getDisplayStatus();
        dispstat &= ~0x7;

        if (scanline >= VDRAW_LINES) {
            dispstat |= 1;
        }

        if (dot >= HDRAW_CYCLES && dot < SCANLINE_CYCLES) {
            dispstat |= 2;
        }

        uint8_t vcountCompare = (dispstat >> 8) & 0xFF;
        if (scanline == vcountCompare) {
            dispstat |= 4;
        }

        mmu.setDisplayStatus(dispstat);
    }
}

void PPU::renderScanline() {
    uint16_t dispcnt = mmu.getDisplayControl();
    uint8_t mode = dispcnt & 0x7;

    switch (mode) {
        case 0:
            renderMode0();
            break;
        case 3:
            renderMode3();
            break;
        case 4:
            renderMode4();
            break;
        case 5:
            renderMode5();
            break;
        default:
            for (int x = 0; x < SCREEN_WIDTH; x++) {
                framebuffer[scanline * SCREEN_WIDTH + x] = 0xFF000000;
            }
            break;
    }
}

void PPU::renderMode0() {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        framebuffer[scanline * SCREEN_WIDTH + x] = 0xFF000000;
    }
}

void PPU::renderMode3() {
    uint8_t* vram = mmu.getVRAM();

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint32_t offset = (scanline * SCREEN_WIDTH + x) * 2;
        uint16_t color = vram[offset] | (vram[offset + 1] << 8);
        framebuffer[scanline * SCREEN_WIDTH + x] = rgb15to32(color);
    }
}

void PPU::renderMode4() {
    uint16_t dispcnt = mmu.getDisplayControl();
    bool frame1 = (dispcnt >> 4) & 1;

    uint8_t* vram = mmu.getVRAM();
    uint8_t* palette = mmu.getPalette();

    uint32_t baseAddr = frame1 ? 0xA000 : 0;

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint32_t offset = baseAddr + scanline * SCREEN_WIDTH + x;
        uint8_t paletteIndex = vram[offset];

        uint16_t color = palette[paletteIndex * 2] | (palette[paletteIndex * 2 + 1] << 8);
        framebuffer[scanline * SCREEN_WIDTH + x] = rgb15to32(color);
    }
}

void PPU::renderMode5() {
    uint16_t dispcnt = mmu.getDisplayControl();
    bool frame1 = (dispcnt >> 4) & 1;

    uint8_t* vram = mmu.getVRAM();

    constexpr int MODE5_WIDTH = 160;
    constexpr int MODE5_HEIGHT = 128;

    uint32_t baseAddr = frame1 ? 0xA000 : 0;

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        if (scanline < MODE5_HEIGHT && x < MODE5_WIDTH) {
            uint32_t offset = baseAddr + (scanline * MODE5_WIDTH + x) * 2;
            uint16_t color = vram[offset] | (vram[offset + 1] << 8);
            framebuffer[scanline * SCREEN_WIDTH + x] = rgb15to32(color);
        } else {
            framebuffer[scanline * SCREEN_WIDTH + x] = 0xFF000000;
        }
    }
}

void PPU::renderSprites() {
}

uint32_t PPU::rgb15to32(uint16_t color) {
    uint8_t r = (color & 0x1F) << 3;
    uint8_t g = ((color >> 5) & 0x1F) << 3;
    uint8_t b = ((color >> 10) & 0x1F) << 3;
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}
