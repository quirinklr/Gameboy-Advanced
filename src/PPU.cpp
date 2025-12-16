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
    }

    uint16_t dispstat = mmu.getDisplayStatus();
    dispstat &= 0xFFF8;

    if (scanline >= VDRAW_LINES && scanline < TOTAL_LINES) {
        dispstat |= 1;
    }

    uint8_t vcountCompare = (dispstat >> 8) & 0xFF;
    if (scanline == vcountCompare) {
        dispstat |= 4;
    }

    mmu.setDisplayStatus(dispstat);
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
    uint8_t* palette = mmu.getPalette();
    uint16_t backdropColor = palette[0] | (palette[1] << 8);
    uint32_t backdrop32 = rgb15to32(backdropColor);

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        framebuffer[scanline * SCREEN_WIDTH + x] = backdrop32;
    }

    uint16_t dispcnt = mmu.getDisplayControl();

    for (int priority = 3; priority >= 0; priority--) {
        for (int bg = 3; bg >= 0; bg--) {
            if (dispcnt & (1 << (8 + bg))) {
                uint16_t bgcnt = mmu.getBGControl(bg);
                if ((bgcnt & 3) == priority) {
                    renderBackground(bg);
                }
            }
        }
    }
}

void PPU::renderBackground(int bg) {
    uint16_t bgcnt = mmu.getBGControl(bg);
    uint16_t bghofs = mmu.getBGXOffset(bg);
    uint16_t bgvofs = mmu.getBGYOffset(bg);

    int charBaseBlock = (bgcnt >> 2) & 3;
    int screenBaseBlock = (bgcnt >> 8) & 0x1F;
    bool color256 = (bgcnt >> 7) & 1;
    int screenSize = (bgcnt >> 14) & 3;

    int priority = bgcnt & 3; 

    uint32_t charBase = charBaseBlock * 0x4000;
    uint32_t screenBase = screenBaseBlock * 0x800;

    int width = 256;
    int height = 256;

    if (screenSize == 1) width = 512;
    if (screenSize == 2) height = 512;
    if (screenSize == 3) { width = 512; height = 512; }

    uint8_t* vram = mmu.getVRAM();
    uint8_t* palette = mmu.getPalette();

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        int xx = (x + bghofs) % width;
        int yy = (scanline + bgvofs) % height;

        int screenBlockX = xx / 256;
        int screenBlockY = yy / 256;
        int screenBlockIndex = 0;

        if (screenSize == 0) screenBlockIndex = 0; 
        else if (screenSize == 1) screenBlockIndex = screenBlockX;
        else if (screenSize == 2) screenBlockIndex = screenBlockY; 
        else screenBlockIndex = screenBlockY * 2 + screenBlockX;

        int tileX = (xx % 256) / 8;
        int tileY = (yy % 256) / 8;
        int tileMapIndex = tileY * 32 + tileX;

        uint32_t mapAddr = screenBase + screenBlockIndex * 0x800 + tileMapIndex * 2;
        uint16_t tileData = vram[mapAddr] | (vram[mapAddr + 1] << 8);

        int tileIndex = tileData & 0x3FF;
        bool hFlip = (tileData >> 10) & 1;
        bool vFlip = (tileData >> 11) & 1;
        int paletteBank = (tileData >> 12) & 0xF;

        int tilePixelX = xx % 8;
        int tilePixelY = yy % 8;

        if (hFlip) tilePixelX = 7 - tilePixelX;
        if (vFlip) tilePixelY = 7 - tilePixelY;

        uint8_t param = 0;
        uint32_t colorIndex = 0;

        if (!color256) {
            uint32_t tileAddr = charBase + tileIndex * 32 + (tilePixelY * 4) + (tilePixelX / 2);
            uint8_t byte = vram[tileAddr];
            if (tilePixelX & 1) param = (byte >> 4) & 0xF;
            else param = byte & 0xF;
            
            if (param != 0) {
                colorIndex = paletteBank * 16 + param;
            }
        } else {
            uint32_t tileAddr = charBase + tileIndex * 64 + (tilePixelY * 8) + tilePixelX;
            param = vram[tileAddr];
            if (param != 0) {
                colorIndex = param;
            }
        }

        if (param != 0) {
            uint16_t color = palette[colorIndex * 2] | (palette[colorIndex * 2 + 1] << 8);
            framebuffer[scanline * SCREEN_WIDTH + x] = rgb15to32(color);
        }
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
