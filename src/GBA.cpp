#include "GBA.h"
#include "CPU.h"
#include "MMU.h"
#include "PPU.h"
#include "Timer.h"
#include "DMA.h"
#include "APU.h"

GBA::GBA() {
    mmu = std::make_unique<MMU>();
    ppu = std::make_unique<PPU>(*mmu);
    cpu = std::make_unique<CPU>(*mmu);
    timer = std::make_unique<Timer>(*mmu);
    dma = std::make_unique<DMA>(*mmu);
    apu = std::make_unique<APU>(*mmu);

    mmu->connectPPU(ppu.get());
}

GBA::~GBA() = default;

bool GBA::loadROM(const std::string& path) {
    if (!mmu->loadROM(path)) {
        return false;
    }
    reset();
    return true;
}

void GBA::reset() {
    cpu->reset();
    ppu->reset();
    timer->reset();
    dma->reset();
    apu->reset();
}

void GBA::runFrame() {
    ppu->clearFrameReady();

    while (!ppu->isFrameReady()) {
        cpu->step();
        timer->step(1);
        apu->step(1);
        ppu->step(1);
        cpu->checkIRQ();
    }
}

const uint32_t* GBA::getFramebuffer() const {
    return ppu->getFramebuffer();
}

bool GBA::isFrameReady() const {
    return ppu->isFrameReady();
}

void GBA::clearFrameReady() {
    ppu->clearFrameReady();
}

void GBA::updateKey(int id, bool pressed) {
    static uint16_t currentKeys = 0x03FF;
    if (pressed) {
        currentKeys &= ~(1 << id);
    } else {
        currentKeys |= (1 << id);
    }
    mmu->setKeyInput(currentKeys);
}
