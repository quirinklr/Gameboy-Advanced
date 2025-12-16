#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include "../src/GBA.h"
#include "../src/CPU.h"
#include "../src/MMU.h"
#include "../src/PPU.h"

class TestRunner {
public:
    TestRunner() : mmu(), ppu(mmu), cpu(mmu) {
        mmu.connectPPU(&ppu);
    }

    bool loadROM(const std::string& path) {
        return mmu.loadROM(path);
    }

    void reset() {
        cpu.reset();
        ppu.reset();
    }

    void runCycles(int cycles) {
        for (int i = 0; i < cycles; i++) {
            cpu.step();
            ppu.step(1);
        }
    }

    void dumpState() {
        std::cout << "=== CPU State ===" << std::endl;
        for (int i = 0; i < 16; i++) {
            std::cout << "R" << std::dec << i << ": 0x" 
                      << std::hex << std::setfill('0') << std::setw(8) 
                      << cpu.getRegister(i);
            if (i % 4 == 3) std::cout << std::endl;
            else std::cout << "  ";
        }
        std::cout << "CPSR: 0x" << std::hex << std::setfill('0') << std::setw(8) << cpu.getCPSR() << std::endl;
        std::cout << "Mode: " << (cpu.inThumbMode() ? "Thumb" : "ARM") << std::endl;
    }

    void dumpVRAM(int offset, int count) {
        uint8_t* vram = mmu.getVRAM();
        std::cout << "=== VRAM @ 0x" << std::hex << offset << " ===" << std::endl;
        for (int i = 0; i < count; i++) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)vram[offset + i] << " ";
            if ((i + 1) % 16 == 0) std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    void dumpPalette() {
        uint8_t* palette = mmu.getPalette();
        std::cout << "=== Palette ===" << std::endl;
        for (int i = 0; i < 32; i++) {
            uint16_t color = palette[i*2] | (palette[i*2+1] << 8);
            std::cout << std::hex << std::setfill('0') << std::setw(4) << color << " ";
            if ((i + 1) % 8 == 0) std::cout << std::endl;
        }
    }

    void dumpIORegs() {
        std::cout << "=== I/O Registers ===" << std::endl;
        std::cout << "DISPCNT:  0x" << std::hex << std::setfill('0') << std::setw(4) << mmu.getDisplayControl() << std::endl;
        std::cout << "DISPSTAT: 0x" << std::hex << std::setfill('0') << std::setw(4) << mmu.getDisplayStatus() << std::endl;
        std::cout << "VCOUNT:   0x" << std::hex << std::setfill('0') << std::setw(4) << mmu.getVCount() << std::endl;
    }

    bool checkVRAMNotEmpty() {
        uint8_t* vram = mmu.getVRAM();
        for (int i = 0; i < 0x18000; i++) {
            if (vram[i] != 0) return true;
        }
        return false;
    }

    bool checkPaletteNotEmpty() {
        uint8_t* palette = mmu.getPalette();
        for (int i = 0; i < 0x400; i++) {
            if (palette[i] != 0) return true;
        }
        return false;
    }

    uint32_t getPC() { return cpu.getPC(); }
    uint32_t getRegister(int r) { return cpu.getRegister(r); }

private:
    MMU mmu;
    PPU ppu;
    CPU cpu;
};

void testCPUBasics() {
    std::cout << "\n=== CPU Basic Tests ===" << std::endl;
    
    MMU mmu;
    CPU cpu(mmu);
    
    cpu.reset();
    
    if (cpu.getPC() == 0x08000000) {
        std::cout << "[PASS] PC initialized to 0x08000000" << std::endl;
    } else {
        std::cout << "[FAIL] PC = 0x" << std::hex << cpu.getPC() << std::endl;
    }
    
    if (cpu.getRegister(13) == 0x03007F00) {
        std::cout << "[PASS] SP initialized to 0x03007F00" << std::endl;
    } else {
        std::cout << "[FAIL] SP = 0x" << std::hex << cpu.getRegister(13) << std::endl;
    }
    
    if (!cpu.inThumbMode()) {
        std::cout << "[PASS] CPU starts in ARM mode" << std::endl;
    } else {
        std::cout << "[FAIL] CPU incorrectly in Thumb mode" << std::endl;
    }
}

void testROMExecution(const std::string& romPath) {
    std::cout << "\n=== ROM Execution Test: " << romPath << " ===" << std::endl;
    
    TestRunner runner;
    
    if (!runner.loadROM(romPath)) {
        std::cout << "[FAIL] Could not load ROM" << std::endl;
        return;
    }
    std::cout << "[PASS] ROM loaded" << std::endl;
    
    runner.reset();
    
    std::cout << "\n--- Initial State ---" << std::endl;
    runner.dumpState();
    
    std::cout << "\n--- Running 100 cycles ---" << std::endl;
    runner.runCycles(100);
    runner.dumpState();
    runner.dumpIORegs();
    
    std::cout << "\n--- Running 1000 cycles ---" << std::endl;
    runner.runCycles(1000);
    runner.dumpState();
    runner.dumpIORegs();
    
    std::cout << "\n--- Running 10000 cycles ---" << std::endl;
    runner.runCycles(10000);
    runner.dumpState();
    runner.dumpIORegs();
    
    std::cout << "\n--- Running 2000000 cycles ---" << std::endl;
    runner.runCycles(2000000);
    
    runner.dumpIORegs();
    // Check for success (R7 == 0 for Thumb tests, R12 for ARM)
    uint32_t resultReg = runner.getRegister(7); // R7 is used in thumb.asm
    if (resultReg == 0) {
        std::cout << "SUCCESS: All tests passed!" << std::endl;
    } else {
        std::cout << "FAILURE: Failed at test " << std::dec << resultReg << std::endl;
    }
    
    if (runner.checkVRAMNotEmpty()) {
        std::cout << "[PASS] VRAM has data" << std::endl;
    } else {
        std::cout << "[FAIL] VRAM is empty" << std::endl;
    }
    
    std::cout << "\n--- VRAM Sample (first 64 bytes) ---" << std::endl;
    runner.dumpVRAM(0, 64);
    
    std::cout << "\n--- Final State ---" << std::endl;
    runner.dumpState();
}

int main(int argc, char* argv[]) {
    std::cout << "==============================" << std::endl;
    std::cout << "GBA Emulator Test Suite" << std::endl;
    std::cout << "==============================" << std::endl;
    
    testCPUBasics();
    
    if (argc > 1) {
        testROMExecution(argv[1]);
    } else {
        std::cout << "\nUsage: " << argv[0] << " <rom.gba>" << std::endl;
        std::cout << "Running without ROM tests." << std::endl;
    }
    
    std::cout << "\n==============================" << std::endl;
    std::cout << "Tests Complete" << std::endl;
    std::cout << "==============================" << std::endl;
    
    return 0;
}
