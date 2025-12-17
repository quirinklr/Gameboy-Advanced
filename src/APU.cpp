#include "APU.h"
#include "MMU.h"

APU::APU(MMU& mmu) : mmu(mmu) {
    reset();
}

void APU::reset() {
    registers.fill(0);
    fifoA.fill(0);
    fifoB.fill(0);
    fifoAPos = 0;
    fifoBPos = 0;
    fifoASize = 0;
    fifoBSize = 0;
    cycleCounter = 0;
    sampleBuffer.clear();
    sampleBuffer.reserve(2048);
    
    square1Phase = 0;
    square2Phase = 0;
    wavePhase = 0;
    noiseShift = 0x7FFF;
    
    frameSequencer = 0;
    frameCounter = 0;
    
    square1LengthCounter = 0;
    square2LengthCounter = 0;
    waveLengthCounter = 0;
    noiseLengthCounter = 0;
    
    square1Volume = 0;
    square2Volume = 0;
    noiseVolume = 0;
}

void APU::step(int cycles) {
    cycleCounter += cycles;
    
    while (cycleCounter >= CYCLES_PER_SAMPLE) {
        cycleCounter -= CYCLES_PER_SAMPLE;
        generateSample();
    }
}

void APU::generateSample() {
    uint16_t soundCnt = registers[0x80 >> 1];
    bool masterEnable = soundCnt & 0x80;
    
    if (!masterEnable) {
        sampleBuffer.push_back(0);
        sampleBuffer.push_back(0);
        return;
    }
    
    int16_t sq1 = generateSquare1();
    int16_t sq2 = generateSquare2();
    int16_t wave = generateWave();
    int16_t noise = generateNoise();
    
    int leftPsg = 0;
    int rightPsg = 0;
    
    uint16_t soundCntL = registers[0x80 >> 1];
    
    if (soundCntL & 0x10) leftPsg += sq1;
    if (soundCntL & 0x20) leftPsg += sq2;
    if (soundCntL & 0x40) leftPsg += wave;
    if (soundCntL & 0x80) leftPsg += noise;
    
    if (soundCntL & 0x01) rightPsg += sq1;
    if (soundCntL & 0x02) rightPsg += sq2;
    if (soundCntL & 0x04) rightPsg += wave;
    if (soundCntL & 0x08) rightPsg += noise;
    
    int leftVolume = (soundCntL >> 4) & 0x07;
    int rightVolume = soundCntL & 0x07;
    
    leftPsg = (leftPsg * (leftVolume + 1)) / 8;
    rightPsg = (rightPsg * (rightVolume + 1)) / 8;
    
    int16_t fifoASample = 0;
    int16_t fifoBSample = 0;
    
    if (fifoASize > 0) {
        fifoASample = fifoA[fifoAPos] * 256;
        fifoAPos = (fifoAPos + 1) % 32;
        fifoASize--;
    }
    
    if (fifoBSize > 0) {
        fifoBSample = fifoB[fifoBPos] * 256;
        fifoBPos = (fifoBPos + 1) % 32;
        fifoBSize--;
    }
    
    uint16_t soundCntH = registers[0x82 >> 1];
    
    int left = leftPsg;
    int right = rightPsg;
    
    if (soundCntH & 0x200) left += fifoASample;
    if (soundCntH & 0x100) right += fifoASample;
    if (soundCntH & 0x2000) left += fifoBSample;
    if (soundCntH & 0x1000) right += fifoBSample;
    
    if (left > 32767) left = 32767;
    if (left < -32768) left = -32768;
    if (right > 32767) right = 32767;
    if (right < -32768) right = -32768;
    
    if (sampleBuffer.size() < 4096) {
        sampleBuffer.push_back(static_cast<int16_t>(left));
        sampleBuffer.push_back(static_cast<int16_t>(right));
    }
}

int16_t APU::generateSquare1() {
    uint16_t cnt = registers[0x62 >> 1];
    uint16_t freq = registers[0x64 >> 1];
    
    if (!(freq & 0x8000)) return 0;
    
    int duty = (cnt >> 6) & 0x03;
    int freqVal = freq & 0x7FF;
    
    int period = (2048 - freqVal) * 4;
    if (period == 0) period = 1;
    
    square1Phase = (square1Phase + 1) % period;
    
    int dutyThreshold = period / 8;
    switch (duty) {
        case 0: dutyThreshold = period / 8; break;
        case 1: dutyThreshold = period / 4; break;
        case 2: dutyThreshold = period / 2; break;
        case 3: dutyThreshold = period * 3 / 4; break;
    }
    
    int volume = square1Volume ? square1Volume : ((cnt >> 12) & 0x0F);
    
    return (square1Phase < dutyThreshold) ? (volume * 256) : -(volume * 256);
}

int16_t APU::generateSquare2() {
    uint16_t cnt = registers[0x68 >> 1];
    uint16_t freq = registers[0x6C >> 1];
    
    if (!(freq & 0x8000)) return 0;
    
    int duty = (cnt >> 6) & 0x03;
    int freqVal = freq & 0x7FF;
    
    int period = (2048 - freqVal) * 4;
    if (period == 0) period = 1;
    
    square2Phase = (square2Phase + 1) % period;
    
    int dutyThreshold = period / 8;
    switch (duty) {
        case 0: dutyThreshold = period / 8; break;
        case 1: dutyThreshold = period / 4; break;
        case 2: dutyThreshold = period / 2; break;
        case 3: dutyThreshold = period * 3 / 4; break;
    }
    
    int volume = square2Volume ? square2Volume : ((cnt >> 12) & 0x0F);
    
    return (square2Phase < dutyThreshold) ? (volume * 256) : -(volume * 256);
}

int16_t APU::generateWave() {
    uint16_t cnt = registers[0x70 >> 1];
    uint16_t freq = registers[0x74 >> 1];
    
    if (!(cnt & 0x80)) return 0;
    if (!(freq & 0x8000)) return 0;
    
    int freqVal = freq & 0x7FF;
    int period = (2048 - freqVal) * 2;
    if (period == 0) period = 1;
    
    wavePhase = (wavePhase + 1) % period;
    
    int wavePos = (wavePhase * 32) / period;
    
    int volume = (cnt >> 13) & 0x03;
    if (volume == 0) return 0;
    
    int sample = 8;
    int shift = 4 - volume;
    if (shift < 0) shift = 0;
    
    return (sample >> shift) * 256;
}

int16_t APU::generateNoise() {
    uint16_t cnt = registers[0x78 >> 1];
    uint16_t freq = registers[0x7C >> 1];
    
    if (!(freq & 0x8000)) return 0;
    
    int divider = cnt & 0x07;
    int shift = (cnt >> 4) & 0x0F;
    bool width7 = (cnt >> 3) & 1;
    
    int period = divider == 0 ? 8 : divider * 16;
    period <<= shift;
    if (period == 0) period = 1;
    
    static int noiseCounter = 0;
    noiseCounter++;
    if (noiseCounter >= period) {
        noiseCounter = 0;
        
        int feedback = (noiseShift & 1) ^ ((noiseShift >> 1) & 1);
        noiseShift >>= 1;
        noiseShift |= feedback << 14;
        
        if (width7) {
            noiseShift &= ~(1 << 6);
            noiseShift |= feedback << 6;
        }
    }
    
    int volume = noiseVolume ? noiseVolume : ((cnt >> 12) & 0x0F);
    
    return (noiseShift & 1) ? (volume * 256) : -(volume * 256);
}

uint16_t APU::readRegister(uint32_t address) const {
    uint32_t reg = (address & 0xFF) >> 1;
    if (reg < registers.size()) {
        return registers[reg];
    }
    return 0;
}

void APU::writeRegister(uint32_t address, uint16_t value) {
    uint32_t reg = (address & 0xFF) >> 1;
    if (reg < registers.size()) {
        registers[reg] = value;
    }
}

void APU::writeFifoA(uint32_t value) {
    if (fifoASize < 32) {
        int writePos = (fifoAPos + fifoASize) % 32;
        fifoA[writePos] = value & 0xFF;
        fifoASize++;
    }
    if (fifoASize < 32) {
        int writePos = (fifoAPos + fifoASize) % 32;
        fifoA[writePos] = (value >> 8) & 0xFF;
        fifoASize++;
    }
    if (fifoASize < 32) {
        int writePos = (fifoAPos + fifoASize) % 32;
        fifoA[writePos] = (value >> 16) & 0xFF;
        fifoASize++;
    }
    if (fifoASize < 32) {
        int writePos = (fifoAPos + fifoASize) % 32;
        fifoA[writePos] = (value >> 24) & 0xFF;
        fifoASize++;
    }
}

void APU::writeFifoB(uint32_t value) {
    if (fifoBSize < 32) {
        int writePos = (fifoBPos + fifoBSize) % 32;
        fifoB[writePos] = value & 0xFF;
        fifoBSize++;
    }
    if (fifoBSize < 32) {
        int writePos = (fifoBPos + fifoBSize) % 32;
        fifoB[writePos] = (value >> 8) & 0xFF;
        fifoBSize++;
    }
    if (fifoBSize < 32) {
        int writePos = (fifoBPos + fifoBSize) % 32;
        fifoB[writePos] = (value >> 16) & 0xFF;
        fifoBSize++;
    }
    if (fifoBSize < 32) {
        int writePos = (fifoBPos + fifoBSize) % 32;
        fifoB[writePos] = (value >> 24) & 0xFF;
        fifoBSize++;
    }
}
