#pragma once

#include <cstdint>
#include <array>

class MMU;

enum class CPUMode : uint8_t {
    User       = 0b10000,
    FIQ        = 0b10001,
    IRQ        = 0b10010,
    Supervisor = 0b10011,
    Abort      = 0b10111,
    Undefined  = 0b11011,
    System     = 0b11111
};

class CPU {
public:
    CPU(MMU& mmu);

    void reset();
    void step();

    uint32_t getRegister(int r) const;
    void setRegister(int r, uint32_t value);

    uint32_t getPC() const { return registers[15]; }
    void setPC(uint32_t value) { registers[15] = value; }

    uint32_t getCPSR() const { return cpsr; }
    void setCPSR(uint32_t value) { cpsr = value; }

    bool inThumbMode() const { return cpsr & (1 << 5); }

private:
    void executeARM(uint32_t instruction);
    void executeThumb(uint16_t instruction);

    bool checkCondition(uint32_t instruction);

    void armDataProcessing(uint32_t instruction);
    void armBranch(uint32_t instruction);
    void armBranchExchange(uint32_t instruction);
    void armSingleDataTransfer(uint32_t instruction);
    void armHalfwordDataTransfer(uint32_t instruction);
    void armBlockDataTransfer(uint32_t instruction);
    void armMultiply(uint32_t instruction);
    void armMRS(uint32_t instruction);
    void armMSR(uint32_t instruction);
    void armMSRImm(uint32_t instruction);
    void armSoftwareInterrupt(uint32_t instruction);
    void handleSWI(uint8_t comment);

    void thumbMoveShiftedRegister(uint16_t instruction);
    void thumbAddSubtract(uint16_t instruction);
    void thumbMoveCompareAddSubtract(uint16_t instruction);
    void thumbALUOperations(uint16_t instruction);
    void thumbHiRegisterOps(uint16_t instruction);
    void thumbPCRelativeLoad(uint16_t instruction);
    void thumbLoadStoreRegOffset(uint16_t instruction);
    void thumbLoadStoreSignExtend(uint16_t instruction);
    void thumbLoadStoreImmediate(uint16_t instruction);
    void thumbLoadStoreHalfword(uint16_t instruction);
    void thumbSPRelativeLoadStore(uint16_t instruction);
    void thumbLoadAddress(uint16_t instruction);
    void thumbAddOffsetToSP(uint16_t instruction);
    void thumbPushPop(uint16_t instruction);
    void thumbMultipleLoadStore(uint16_t instruction);
    void thumbConditionalBranch(uint16_t instruction);
    void thumbSoftwareInterrupt(uint16_t instruction);
    void thumbUnconditionalBranch(uint16_t instruction);
    void thumbLongBranchLink(uint16_t instruction);

    void setNZ(uint32_t result);
    void setNZCV(uint32_t result, bool carry, bool overflow);

    uint32_t shiftValue(uint32_t value, int shiftType, int shiftAmount, bool& carryOut);
    uint32_t rotateRight(uint32_t value, int amount);

    MMU& mmu;

    std::array<uint32_t, 16> registers{};
    uint32_t cpsr = 0;
    std::array<uint32_t, 5> spsr{};

    std::array<uint32_t, 7> bankedFIQ{};
    std::array<uint32_t, 2> bankedIRQ{};
    std::array<uint32_t, 2> bankedSVC{};
    std::array<uint32_t, 2> bankedABT{};
    std::array<uint32_t, 2> bankedUND{};

    uint64_t cycles = 0;
};
