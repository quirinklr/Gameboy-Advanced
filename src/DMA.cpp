#include "DMA.h"
#include "MMU.h"

DMA::DMA(MMU& mmu) : mmu(mmu) {
    reset();
}

void DMA::reset() {
    source.fill(0);
    dest.fill(0);
    internalSource.fill(0);
    internalDest.fill(0);
    count.fill(0);
    control.fill(0);
}

void DMA::checkImmediate(int channel) {
    if (!(control[channel] & 0x8000)) return;
    
    int timing = (control[channel] >> 12) & 0x03;
    if (timing == 0) {
        execute(channel);
    }
}

void DMA::triggerVBlank() {
    for (int i = 0; i < 4; i++) {
        if (!(control[i] & 0x8000)) continue;
        int timing = (control[i] >> 12) & 0x03;
        if (timing == 1) {
            execute(i);
        }
    }
}

void DMA::triggerHBlank() {
    for (int i = 0; i < 4; i++) {
        if (!(control[i] & 0x8000)) continue;
        int timing = (control[i] >> 12) & 0x03;
        if (timing == 2) {
            execute(i);
        }
    }
}

void DMA::execute(int channel) {
    bool is32bit = (control[channel] >> 10) & 1;
    int srcMode = (control[channel] >> 7) & 0x03;
    int dstMode = (control[channel] >> 5) & 0x03;
    bool repeat = (control[channel] >> 9) & 1;
    
    uint32_t transferCount = count[channel];
    if (transferCount == 0) {
        transferCount = (channel == 3) ? 0x10000 : 0x4000;
    }
    
    int srcIncrement = is32bit ? 4 : 2;
    int dstIncrement = is32bit ? 4 : 2;
    
    if (srcMode == 1) srcIncrement = -srcIncrement;
    else if (srcMode == 2) srcIncrement = 0;
    
    if (dstMode == 1) dstIncrement = -dstIncrement;
    else if (dstMode == 2) dstIncrement = 0;
    
    for (uint32_t i = 0; i < transferCount; i++) {
        if (is32bit) {
            uint32_t value = mmu.read32(internalSource[channel]);
            mmu.write32(internalDest[channel], value);
        } else {
            uint16_t value = mmu.read16(internalSource[channel]);
            mmu.write16(internalDest[channel], value);
        }
        
        internalSource[channel] += srcIncrement;
        internalDest[channel] += dstIncrement;
    }
    
    if (control[channel] & 0x4000) {
        uint16_t if_ = mmu.getIF();
        mmu.setIF(if_ | (1 << (8 + channel)));
    }
    
    if (repeat && ((control[channel] >> 12) & 0x03) != 0) {
        if (dstMode == 3) {
            internalDest[channel] = dest[channel];
        }
    } else {
        control[channel] &= ~0x8000;
    }
}

uint32_t DMA::readSource(int channel) const {
    return source[channel];
}

uint32_t DMA::readDest(int channel) const {
    return dest[channel];
}

uint16_t DMA::readCount(int channel) const {
    return count[channel];
}

uint16_t DMA::readControl(int channel) const {
    return control[channel];
}

void DMA::writeSource(int channel, uint32_t value, bool high) {
    if (high) {
        source[channel] = (source[channel] & 0xFFFF) | (value << 16);
    } else {
        source[channel] = (source[channel] & 0xFFFF0000) | (value & 0xFFFF);
    }
}

void DMA::writeDest(int channel, uint32_t value, bool high) {
    if (high) {
        dest[channel] = (dest[channel] & 0xFFFF) | (value << 16);
    } else {
        dest[channel] = (dest[channel] & 0xFFFF0000) | (value & 0xFFFF);
    }
}

void DMA::writeCount(int channel, uint16_t value) {
    count[channel] = value;
}

void DMA::writeControl(int channel, uint16_t value) {
    bool wasEnabled = control[channel] & 0x8000;
    bool nowEnabled = value & 0x8000;
    
    control[channel] = value;
    
    if (!wasEnabled && nowEnabled) {
        internalSource[channel] = source[channel];
        internalDest[channel] = dest[channel];
        checkImmediate(channel);
    }
}
