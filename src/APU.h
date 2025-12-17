#pragma once

#include <cstdint>
#include <array>
#include <vector>

class MMU;

class APU {
public:
    APU(MMU& mmu);
    
    void reset();
    void step(int cycles);
    
    const std::vector<int16_t>& getSampleBuffer() const { return sampleBuffer; }
    void clearSampleBuffer() { sampleBuffer.clear(); }
    
    uint16_t readRegister(uint32_t address) const;
    void writeRegister(uint32_t address, uint16_t value);
    
    void writeFifoA(uint32_t value);
    void writeFifoB(uint32_t value);
    
private:
    void generateSample();
    int16_t generateSquare1();
    int16_t generateSquare2();
    int16_t generateWave();
    int16_t generateNoise();
    
    MMU& mmu;
    
    std::array<uint16_t, 0x18> registers{};
    
    std::array<int8_t, 32> fifoA{};
    std::array<int8_t, 32> fifoB{};
    int fifoAPos = 0;
    int fifoBPos = 0;
    int fifoASize = 0;
    int fifoBSize = 0;
    
    int cycleCounter = 0;
    static constexpr int CYCLES_PER_SAMPLE = 512;
    
    std::vector<int16_t> sampleBuffer;
    
    int square1Phase = 0;
    int square2Phase = 0;
    int wavePhase = 0;
    uint16_t noiseShift = 0x7FFF;
    
    int frameSequencer = 0;
    int frameCounter = 0;
    
    int square1LengthCounter = 0;
    int square2LengthCounter = 0;
    int waveLengthCounter = 0;
    int noiseLengthCounter = 0;
    
    int square1EnvelopeCounter = 0;
    int square2EnvelopeCounter = 0;
    int noiseEnvelopeCounter = 0;
    
    int square1Volume = 0;
    int square2Volume = 0;
    int noiseVolume = 0;
};
