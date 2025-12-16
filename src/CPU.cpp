#include "CPU.h"
#include "MMU.h"
#include "Utils.h"

CPU::CPU(MMU& mmu) : mmu(mmu) {
    reset();
}

void CPU::reset() {
    registers.fill(0);
    cpsr = static_cast<uint32_t>(CPUMode::System);
    spsr.fill(0);
    bankedFIQ.fill(0);
    bankedIRQ.fill(0);
    bankedSVC.fill(0);
    bankedABT.fill(0);
    bankedUND.fill(0);
    cycles = 0;

    registers[15] = 0x08000000;
    registers[13] = 0x03007F00;
}

void CPU::step() {
    if (inThumbMode()) {
        uint16_t instruction = mmu.read16(registers[15]);
        registers[15] += 2;
        executeThumb(instruction);
    } else {
        uint32_t instruction = mmu.read32(registers[15]);
        registers[15] += 4;
        executeARM(instruction);
    }
    cycles++;
}

uint32_t CPU::getRegister(int r) const {
    return registers[r];
}

void CPU::setRegister(int r, uint32_t value) {
    registers[r] = value;
}

bool CPU::checkCondition(uint32_t instruction) {
    uint8_t cond = (instruction >> 28) & 0xF;
    bool N = (cpsr >> 31) & 1;
    bool Z = (cpsr >> 30) & 1;
    bool C = (cpsr >> 29) & 1;
    bool V = (cpsr >> 28) & 1;

    switch (cond) {
        case 0x0: return Z;
        case 0x1: return !Z;
        case 0x2: return C;
        case 0x3: return !C;
        case 0x4: return N;
        case 0x5: return !N;
        case 0x6: return V;
        case 0x7: return !V;
        case 0x8: return C && !Z;
        case 0x9: return !C || Z;
        case 0xA: return N == V;
        case 0xB: return N != V;
        case 0xC: return !Z && (N == V);
        case 0xD: return Z || (N != V);
        case 0xE: return true;
        case 0xF: return true;
    }
    return false;
}

void CPU::executeARM(uint32_t instruction) {
    if (!checkCondition(instruction)) {
        return;
    }

    uint32_t bits74 = (instruction >> 4) & 0xF;

    if ((instruction & 0x0FFFFFF0) == 0x012FFF10) {
        armBranchExchange(instruction);
    } else if ((instruction & 0x0FBF0FFF) == 0x010F0000) {
        armMRS(instruction);
    } else if ((instruction & 0x0FB0FFF0) == 0x0120F000) {
        armMSR(instruction);
    } else if ((instruction & 0x0FB0F000) == 0x0320F000) {
        armMSRImm(instruction);
    } else if ((instruction & 0x0E000000) == 0x0A000000) {
        armBranch(instruction);
    } else if ((instruction & 0x0FC000F0) == 0x00000090) {
        armMultiply(instruction);
    } else if ((instruction & 0x0C000000) == 0x04000000) {
        armSingleDataTransfer(instruction);
    } else if ((instruction & 0x0E000090) == 0x00000090 && (bits74 == 0xB || bits74 == 0xD || bits74 == 0xF)) {
        armHalfwordDataTransfer(instruction);
    } else if ((instruction & 0x0E000000) == 0x08000000) {
        armBlockDataTransfer(instruction);
    } else if ((instruction & 0x0F000000) == 0x0F000000) {
        armSoftwareInterrupt(instruction);
    } else if ((instruction & 0x0C000000) == 0x00000000) {
        armDataProcessing(instruction);
    }
}

void CPU::armDataProcessing(uint32_t instruction) {
    bool I = (instruction >> 25) & 1;
    uint8_t opcode_dp = (instruction >> 21) & 0xF;
    bool S = (instruction >> 20) & 1;
    uint8_t Rn = (instruction >> 16) & 0xF;
    uint8_t Rd = (instruction >> 12) & 0xF;

    uint32_t op1 = registers[Rn];
    if (Rn == 15) op1 += 4;
    
    uint32_t op2;
    bool shiftCarry = (cpsr >> 29) & 1;

    if (I) {
        uint8_t imm = instruction & 0xFF;
        uint8_t rotate = ((instruction >> 8) & 0xF) * 2;
        op2 = rotateRight(imm, rotate);
        if (rotate != 0) {
            shiftCarry = (op2 >> 31) & 1;
        }
    } else {
        uint8_t Rm = instruction & 0xF;
        uint8_t shiftType = (instruction >> 5) & 3;
        int shiftAmount;
        bool isRRX = false;

        uint32_t rmVal = registers[Rm];
        if (Rm == 15) rmVal += 4;

        if ((instruction >> 4) & 1) {
            uint8_t Rs = (instruction >> 8) & 0xF;
            shiftAmount = registers[Rs] & 0xFF;
        } else {
            shiftAmount = (instruction >> 7) & 0x1F;
            if (shiftAmount == 0) {
                if (shiftType == 1 || shiftType == 2) {
                    shiftAmount = 32;
                } else if (shiftType == 3) {
                    bool oldCarry = (cpsr >> 29) & 1;
                    op2 = (oldCarry << 31) | (rmVal >> 1);
                    shiftCarry = rmVal & 1;
                    isRRX = true;
                }
            }
        }

        if (!isRRX) {
            op2 = shiftValue(rmVal, shiftType, shiftAmount, shiftCarry);
        }
    }

    uint32_t result = 0;
    bool writeResult = true;
    bool carry = false;
    bool overflow = false;

    switch (opcode_dp) {
        case 0x0:
            result = op1 & op2;
            break;
        case 0x1:
            result = op1 ^ op2;
            break;
        case 0x2: {
            uint64_t diff = (uint64_t)op1 - op2;
            result = (uint32_t)diff;
            carry = op1 >= op2;
            overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            break;
        }
        case 0x3: {
            uint64_t diff = (uint64_t)op2 - op1;
            result = (uint32_t)diff;
            carry = op2 >= op1;
            overflow = ((op2 ^ op1) & (op2 ^ result)) >> 31;
            break;
        }
        case 0x4: {
            uint64_t sum = (uint64_t)op1 + op2;
            result = (uint32_t)sum;
            carry = sum > 0xFFFFFFFF;
            overflow = (~(op1 ^ op2) & (op1 ^ result)) >> 31;
            break;
        }
        case 0x5: {
            uint32_t c = (cpsr >> 29) & 1;
            uint64_t sum = (uint64_t)op1 + op2 + c;
            result = (uint32_t)sum;
            carry = sum > 0xFFFFFFFF;
            overflow = (~(op1 ^ op2) & (op1 ^ result)) >> 31;
            break;
        }
        case 0x6: {
            uint32_t c = (cpsr >> 29) & 1;
            uint64_t diff = (uint64_t)op1 - op2 - !c;
            result = (uint32_t)diff;
            carry = op1 >= op2 + !c;
            overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            break;
        }
        case 0x7: {
            uint32_t c = (cpsr >> 29) & 1;
            uint64_t diff = (uint64_t)op2 - op1 - !c;
            result = (uint32_t)diff;
            carry = op2 >= op1 + !c;
            overflow = ((op2 ^ op1) & (op2 ^ result)) >> 31;
            break;
        }
        case 0x8:
            result = op1 & op2;
            writeResult = false;
            break;
        case 0x9:
            result = op1 ^ op2;
            writeResult = false;
            break;
        case 0xA: {
            uint64_t diff = (uint64_t)op1 - op2;
            result = (uint32_t)diff;
            carry = op1 >= op2;
            overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            writeResult = false;
            break;
        }
        case 0xB: {
            uint64_t sum = (uint64_t)op1 + op2;
            result = (uint32_t)sum;
            carry = sum > 0xFFFFFFFF;
            overflow = (~(op1 ^ op2) & (op1 ^ result)) >> 31;
            writeResult = false;
            break;
        }
        case 0xC:
            result = op1 | op2;
            break;
        case 0xD:
            result = op2;
            break;
        case 0xE:
            result = op1 & ~op2;
            break;
        case 0xF:
            result = ~op2;
            break;
    }

    if (writeResult) {
        registers[Rd] = result;
        if (Rd == 15 && S) {
            int idx = getSPSRIndex();
            if (idx >= 0) {
                uint32_t newCPSR = spsr[idx];
                if ((newCPSR & 0x1F) != (cpsr & 0x1F)) {
                    switchMode(static_cast<CPUMode>(newCPSR & 0x1F));
                }
                cpsr = newCPSR;
            }
        }
    }

    if (S && Rd != 15) {
        if (opcode_dp >= 0x8 && opcode_dp <= 0xB) {
            setNZCV(result, carry, overflow);
        } else if (opcode_dp <= 0x1 || opcode_dp >= 0xC) {
            setNZ(result);
            cpsr = (cpsr & ~(1 << 29)) | (shiftCarry << 29);
        } else {
            setNZCV(result, carry, overflow);
        }
    }
}

void CPU::armBranch(uint32_t instruction) {
    bool L = (instruction >> 24) & 1;
    int32_t offset = instruction & 0x00FFFFFF;

    if (offset & 0x00800000) {
        offset |= 0xFF000000;
    }
    offset <<= 2;

    if (L) {
        registers[14] = registers[15];
    }

    registers[15] += offset + 4;
}

void CPU::armBranchExchange(uint32_t instruction) {
    uint8_t Rn = instruction & 0xF;
    uint32_t address = registers[Rn];

    if (address & 1) {
        cpsr |= (1 << 5);
        registers[15] = address & ~1;
    } else {
        cpsr &= ~(1 << 5);
        registers[15] = address & ~3;
    }
}

void CPU::armSingleDataTransfer(uint32_t instruction) {
    bool I = (instruction >> 25) & 1;
    bool P = (instruction >> 24) & 1;
    bool U = (instruction >> 23) & 1;
    bool B = (instruction >> 22) & 1;
    bool W = (instruction >> 21) & 1;
    bool L = (instruction >> 20) & 1;
    uint8_t Rn = (instruction >> 16) & 0xF;
    uint8_t Rd = (instruction >> 12) & 0xF;

    uint32_t offset;
    if (I) {
        uint8_t Rm = instruction & 0xF;
        uint8_t shiftType = (instruction >> 5) & 3;
        int shiftAmount = (instruction >> 7) & 0x1F;
        bool carry;
        offset = shiftValue(registers[Rm], shiftType, shiftAmount, carry);
    } else {
        offset = instruction & 0xFFF;
    }

    uint32_t base = registers[Rn];
    uint32_t address = base;

    if (P) {
        address = U ? base + offset : base - offset;
    }

    if (L) {
        if (B) {
            registers[Rd] = mmu.read8(address);
        } else {
            registers[Rd] = mmu.read32(address);
        }
    } else {
        if (B) {
            mmu.write8(address, registers[Rd] & 0xFF);
        } else {
            mmu.write32(address, registers[Rd]);
        }
    }

    if (!P) {
        address = U ? base + offset : base - offset;
        registers[Rn] = address;
    } else if (W) {
        registers[Rn] = address;
    }
}

void CPU::armHalfwordDataTransfer(uint32_t instruction) {
    bool P = (instruction >> 24) & 1;
    bool U = (instruction >> 23) & 1;
    bool I = (instruction >> 22) & 1;
    bool W = (instruction >> 21) & 1;
    bool L = (instruction >> 20) & 1;
    uint8_t Rn = (instruction >> 16) & 0xF;
    uint8_t Rd = (instruction >> 12) & 0xF;
    uint8_t SH = (instruction >> 5) & 3;

    uint32_t offset;
    if (I) {
        offset = ((instruction >> 4) & 0xF0) | (instruction & 0xF);
    } else {
        uint8_t Rm = instruction & 0xF;
        offset = registers[Rm];
    }

    uint32_t base = registers[Rn];
    uint32_t address = base;

    if (P) {
        address = U ? base + offset : base - offset;
    }

    if (L) {
        switch (SH) {
            case 1:
                registers[Rd] = mmu.read16(address);
                break;
            case 2: {
                int8_t val = mmu.read8(address);
                registers[Rd] = (uint32_t)(int32_t)val;
                break;
            }
            case 3: {
                int16_t val = mmu.read16(address);
                registers[Rd] = (uint32_t)(int32_t)val;
                break;
            }
        }
    } else {
        if (SH == 1) {
            mmu.write16(address, registers[Rd] & 0xFFFF);
        }
    }

    if (!P) {
        address = U ? base + offset : base - offset;
        registers[Rn] = address;
    } else if (W) {
        registers[Rn] = address;
    }
}

void CPU::armBlockDataTransfer(uint32_t instruction) {
    bool P = (instruction >> 24) & 1;
    bool U = (instruction >> 23) & 1;
    bool S = (instruction >> 22) & 1;
    bool W = (instruction >> 21) & 1;
    bool L = (instruction >> 20) & 1;
    uint8_t Rn = (instruction >> 16) & 0xF;
    uint16_t regList = instruction & 0xFFFF;

    (void)S;

    uint32_t base = registers[Rn];
    int count = 0;
    for (int i = 0; i < 16; i++) {
        if (regList & (1 << i)) count++;
    }

    uint32_t address;
    if (U) {
        address = P ? base + 4 : base;
    } else {
        address = P ? base - count * 4 : base - count * 4 + 4;
    }

    for (int i = 0; i < 16; i++) {
        if (regList & (1 << i)) {
            if (L) {
                registers[i] = mmu.read32(address);
            } else {
                mmu.write32(address, registers[i]);
            }
            address += 4;
        }
    }

    if (W) {
        if (U) {
            registers[Rn] = base + count * 4;
        } else {
            registers[Rn] = base - count * 4;
        }
    }
}

void CPU::armMultiply(uint32_t instruction) {
    bool A = (instruction >> 21) & 1;
    bool S = (instruction >> 20) & 1;
    uint8_t Rd = (instruction >> 16) & 0xF;
    uint8_t Rn = (instruction >> 12) & 0xF;
    uint8_t Rs = (instruction >> 8) & 0xF;
    uint8_t Rm = instruction & 0xF;

    uint32_t result = registers[Rm] * registers[Rs];
    if (A) {
        result += registers[Rn];
    }

    registers[Rd] = result;

    if (S) {
        setNZ(result);
    }
}

void CPU::armMRS(uint32_t instruction) {
    bool useSPSR = (instruction >> 22) & 1;
    uint8_t Rd = (instruction >> 12) & 0xF;
    
    if (useSPSR) {
        registers[Rd] = spsr[0];
    } else {
        registers[Rd] = cpsr;
    }
}

void CPU::armMSR(uint32_t instruction) {
    bool useSPSR = (instruction >> 22) & 1;
    uint8_t Rm = instruction & 0xF;
    uint32_t value = registers[Rm];
    uint32_t mask = 0;
    
    if ((instruction >> 16) & 1) mask |= 0x000000FF;
    if ((instruction >> 17) & 1) mask |= 0x0000FF00;
    if ((instruction >> 18) & 1) mask |= 0x00FF0000;
    if ((instruction >> 19) & 1) mask |= 0xFF000000;
    
    if (useSPSR) {
        int idx = getSPSRIndex();
        if (idx >= 0) spsr[idx] = (spsr[idx] & ~mask) | (value & mask);
    } else {
        uint32_t newCPSR = (cpsr & ~mask) | (value & mask);
        if ((mask & 0x1F) && ((newCPSR & 0x1F) != (cpsr & 0x1F))) {
            switchMode(static_cast<CPUMode>(newCPSR & 0x1F));
        }
        cpsr = newCPSR;
    }
}

void CPU::armMSRImm(uint32_t instruction) {
    bool useSPSR = (instruction >> 22) & 1;
    uint8_t imm = instruction & 0xFF;
    uint8_t rotate = ((instruction >> 8) & 0xF) * 2;
    uint32_t value = rotateRight(imm, rotate);
    uint32_t mask = 0;
    
    if ((instruction >> 16) & 1) mask |= 0x000000FF;
    if ((instruction >> 17) & 1) mask |= 0x0000FF00;
    if ((instruction >> 18) & 1) mask |= 0x00FF0000;
    if ((instruction >> 19) & 1) mask |= 0xFF000000;
    
    if (useSPSR) {
        int idx = getSPSRIndex();
        if (idx >= 0) spsr[idx] = (spsr[idx] & ~mask) | (value & mask);
    } else {
        uint32_t newCPSR = (cpsr & ~mask) | (value & mask);
        if ((mask & 0x1F) && ((newCPSR & 0x1F) != (cpsr & 0x1F))) {
            switchMode(static_cast<CPUMode>(newCPSR & 0x1F));
        }
        cpsr = newCPSR;
    }
}

void CPU::armSoftwareInterrupt(uint32_t instruction) {
    uint8_t comment = (instruction >> 16) & 0xFF;
    handleSWI(comment);
}

void CPU::handleSWI(uint8_t comment) {
    switch (comment) {
        case 0x00:
            break;
        case 0x01:
            break;
        case 0x02:
            break;
        case 0x05: {
            int16_t a = static_cast<int16_t>(registers[0] & 0xFFFF);
            int16_t b = static_cast<int16_t>(registers[1] & 0xFFFF);
            if (a < 0) a = -a;
            if (b < 0) b = -b;
            while (b != 0) {
                int16_t t = b;
                b = a % b;
                a = t;
            }
            registers[0] = a;
            break;
        }
        case 0x06: {
            int32_t numerator = static_cast<int32_t>(registers[0]);
            int32_t denominator = static_cast<int32_t>(registers[1]);
            if (denominator == 0) {
                break;
            }
            int32_t quotient = numerator / denominator;
            int32_t remainder = numerator % denominator;
            registers[0] = static_cast<uint32_t>(quotient);
            registers[1] = static_cast<uint32_t>(remainder);
            registers[3] = static_cast<uint32_t>(quotient < 0 ? -quotient : quotient);
            break;
        }
        case 0x07: {
            int32_t numerator = static_cast<int32_t>(registers[1]);
            int32_t denominator = static_cast<int32_t>(registers[0]);
            if (denominator == 0) {
                break;
            }
            int32_t quotient = numerator / denominator;
            int32_t remainder = numerator % denominator;
            registers[0] = static_cast<uint32_t>(quotient);
            registers[1] = static_cast<uint32_t>(remainder);
            registers[3] = static_cast<uint32_t>(quotient < 0 ? -quotient : quotient);
            break;
        }
        case 0x08: {
            uint32_t n = registers[0];
            if (n == 0) {
                registers[0] = 0;
                break;
            }
            uint32_t x = n;
            uint32_t y = (x + 1) >> 1;
            while (y < x) {
                x = y;
                y = (x + n / x) >> 1;
            }
            registers[0] = x;
            break;
        }
        case 0x09: {
            uint16_t theta = registers[0] & 0xFFFF;
            uint32_t result;
            int idx = (theta >> 6) & 0xFF;
            static const int16_t sincosTable[256] = {
                0, 402, 804, 1206, 1607, 2009, 2410, 2811,
                3211, 3611, 4011, 4409, 4808, 5205, 5602, 5997,
                6392, 6786, 7179, 7571, 7961, 8351, 8739, 9126,
                9512, 9896, 10278, 10659, 11039, 11416, 11793, 12167,
                12539, 12910, 13278, 13645, 14010, 14372, 14732, 15090,
                15446, 15800, 16151, 16499, 16846, 17189, 17530, 17869,
                18204, 18537, 18868, 19195, 19519, 19841, 20159, 20475,
                20787, 21097, 21403, 21706, 22005, 22301, 22594, 22884,
                23170, 23453, 23732, 24007, 24279, 24547, 24812, 25073,
                25330, 25583, 25832, 26077, 26319, 26557, 26790, 27020,
                27245, 27466, 27684, 27897, 28106, 28310, 28511, 28707,
                28898, 29086, 29269, 29447, 29621, 29791, 29956, 30117,
                30273, 30425, 30572, 30714, 30852, 30985, 31114, 31237,
                31357, 31471, 31581, 31685, 31785, 31881, 31971, 32057,
                32138, 32214, 32285, 32351, 32413, 32469, 32521, 32568,
                32610, 32647, 32679, 32706, 32728, 32745, 32758, 32765,
                32767, 32765, 32758, 32745, 32728, 32706, 32679, 32647,
                32610, 32568, 32521, 32469, 32413, 32351, 32285, 32214,
                32138, 32057, 31971, 31881, 31785, 31685, 31581, 31471,
                31357, 31237, 31114, 30985, 30852, 30714, 30572, 30425,
                30273, 30117, 29956, 29791, 29621, 29447, 29269, 29086,
                28898, 28707, 28511, 28310, 28106, 27897, 27684, 27466,
                27245, 27020, 26790, 26557, 26319, 26077, 25832, 25583,
                25330, 25073, 24812, 24547, 24279, 24007, 23732, 23453,
                23170, 22884, 22594, 22301, 22005, 21706, 21403, 21097,
                20787, 20475, 20159, 19841, 19519, 19195, 18868, 18537,
                18204, 17869, 17530, 17189, 16846, 16499, 16151, 15800,
                15446, 15090, 14732, 14372, 14010, 13645, 13278, 12910,
                12539, 12167, 11793, 11416, 11039, 10659, 10278, 9896,
                9512, 9126, 8739, 8351, 7961, 7571, 7179, 6786,
                6392, 5997, 5602, 5205, 4808, 4409, 4011, 3611,
                3211, 2811, 2410, 2009, 1607, 1206, 804, 402
            };
            result = sincosTable[idx];
            registers[0] = result;
            break;
        }
        case 0x0B:
        case 0x0C: {
            uint32_t src = registers[0];
            uint32_t dst = registers[1];
            uint32_t cnt = registers[2];
            bool fill = (cnt >> 24) & 1;
            bool word = (cnt >> 26) & 1;
            uint32_t count = cnt & 0x1FFFFF;
            
            if (word) {
                uint32_t value = mmu.read32(src);
                for (uint32_t i = 0; i < count; i++) {
                    mmu.write32(dst, value);
                    dst += 4;
                    if (!fill) {
                        src += 4;
                        value = mmu.read32(src);
                    }
                }
            } else {
                uint16_t value = mmu.read16(src);
                for (uint32_t i = 0; i < count; i++) {
                    mmu.write16(dst, value);
                    dst += 2;
                    if (!fill) {
                        src += 2;
                        value = mmu.read16(src);
                    }
                }
            }
            break;
        }
        default:
            break;
    }
}

void CPU::executeThumb(uint16_t instruction) {
    if ((instruction >> 13) == 0) {
        if (((instruction >> 11) & 3) == 3) {
            thumbAddSubtract(instruction);
        } else {
            thumbMoveShiftedRegister(instruction);
        }
    } else if ((instruction >> 13) == 1) {
        thumbMoveCompareAddSubtract(instruction);
    } else if ((instruction >> 10) == 0x10) {
        thumbALUOperations(instruction);
    } else if ((instruction >> 10) == 0x11) {
        thumbHiRegisterOps(instruction);
    } else if ((instruction >> 11) == 9) {
        thumbPCRelativeLoad(instruction);
    } else if ((instruction >> 12) == 5) {
        if ((instruction >> 9) & 1) {
            thumbLoadStoreSignExtend(instruction);
        } else {
            thumbLoadStoreRegOffset(instruction);
        }
    } else if ((instruction >> 13) == 3) {
        thumbLoadStoreImmediate(instruction);
    } else if ((instruction >> 12) == 8) {
        thumbLoadStoreHalfword(instruction);
    } else if ((instruction >> 12) == 9) {
        thumbSPRelativeLoadStore(instruction);
    } else if ((instruction >> 12) == 10) {
        thumbLoadAddress(instruction);
    } else if ((instruction >> 8) == 0xB0) {
        thumbAddOffsetToSP(instruction);
    } else if ((instruction >> 12) == 11 && ((instruction >> 9) & 3) == 2) {
        thumbPushPop(instruction);
    } else if ((instruction >> 12) == 12) {
        thumbMultipleLoadStore(instruction);
    } else if ((instruction >> 12) == 13) {
        if (((instruction >> 8) & 0xF) == 0xF) {
            thumbSoftwareInterrupt(instruction);
        } else {
            thumbConditionalBranch(instruction);
        }
    } else if ((instruction >> 11) == 28) {
        thumbUnconditionalBranch(instruction);
    } else if ((instruction >> 12) == 15) {
        thumbLongBranchLink(instruction);
    }
}

void CPU::thumbMoveShiftedRegister(uint16_t instruction) {
    uint8_t op = (instruction >> 11) & 3;
    uint8_t offset = (instruction >> 6) & 0x1F;
    uint8_t Rs = (instruction >> 3) & 7;
    uint8_t Rd = instruction & 7;

    bool carry = (cpsr >> 29) & 1;
    uint32_t result;
    
    if (op == 0 && offset == 0) {
        result = registers[Rs];
    } else if (op != 0 && offset == 0) {
        result = shiftValue(registers[Rs], op, 32, carry);
    } else {
        result = shiftValue(registers[Rs], op, offset, carry);
    }

    registers[Rd] = result;
    setNZ(result);
    cpsr = (cpsr & ~(1 << 29)) | (carry << 29);
}

void CPU::thumbAddSubtract(uint16_t instruction) {
    bool I = (instruction >> 10) & 1;
    bool op = (instruction >> 9) & 1;
    uint8_t RnOrImm = (instruction >> 6) & 7;
    uint8_t Rs = (instruction >> 3) & 7;
    uint8_t Rd = instruction & 7;

    uint32_t operand = I ? RnOrImm : registers[RnOrImm];
    uint32_t result;
    bool carry, overflow;

    if (op) {
        uint64_t diff = (uint64_t)registers[Rs] - operand;
        result = (uint32_t)diff;
        carry = registers[Rs] >= operand;
        overflow = ((registers[Rs] ^ operand) & (registers[Rs] ^ result)) >> 31;
    } else {
        uint64_t sum = (uint64_t)registers[Rs] + operand;
        result = (uint32_t)sum;
        carry = sum > 0xFFFFFFFF;
        overflow = (~(registers[Rs] ^ operand) & (registers[Rs] ^ result)) >> 31;
    }

    registers[Rd] = result;
    setNZCV(result, carry, overflow);
}

void CPU::thumbMoveCompareAddSubtract(uint16_t instruction) {
    uint8_t op = (instruction >> 11) & 3;
    uint8_t Rd = (instruction >> 8) & 7;
    uint8_t imm = instruction & 0xFF;

    uint32_t result;
    bool carry = false, overflow = false;

    switch (op) {
        case 0:
            result = imm;
            registers[Rd] = result;
            setNZ(result);
            return;
        case 1: {
            uint64_t diff = (uint64_t)registers[Rd] - imm;
            result = (uint32_t)diff;
            carry = registers[Rd] >= imm;
            overflow = ((registers[Rd] ^ imm) & (registers[Rd] ^ result)) >> 31;
            break;
        }
        case 2: {
            uint64_t sum = (uint64_t)registers[Rd] + imm;
            result = (uint32_t)sum;
            carry = sum > 0xFFFFFFFF;
            overflow = (~(registers[Rd] ^ imm) & (registers[Rd] ^ result)) >> 31;
            registers[Rd] = result;
            break;
        }
        case 3: {
            uint64_t diff = (uint64_t)registers[Rd] - imm;
            result = (uint32_t)diff;
            carry = registers[Rd] >= imm;
            overflow = ((registers[Rd] ^ imm) & (registers[Rd] ^ result)) >> 31;
            registers[Rd] = result;
            break;
        }
    }
    setNZCV(result, carry, overflow);
}

void CPU::thumbALUOperations(uint16_t instruction) {
    uint8_t op = (instruction >> 6) & 0xF;
    uint8_t Rs = (instruction >> 3) & 7;
    uint8_t Rd = instruction & 7;

    uint32_t result;
    bool carry = (cpsr >> 29) & 1;
    bool overflow = (cpsr >> 28) & 1;

    switch (op) {
        case 0x0:
            result = registers[Rd] & registers[Rs];
            break;
        case 0x1:
            result = registers[Rd] ^ registers[Rs];
            break;
        case 0x2:
            result = shiftValue(registers[Rd], 0, registers[Rs] & 0xFF, carry);
            break;
        case 0x3:
            result = shiftValue(registers[Rd], 1, registers[Rs] & 0xFF, carry);
            break;
        case 0x4:
            result = shiftValue(registers[Rd], 2, registers[Rs] & 0xFF, carry);
            break;
        case 0x5: {
            uint32_t c = (cpsr >> 29) & 1;
            uint64_t sum = (uint64_t)registers[Rd] + registers[Rs] + c;
            result = (uint32_t)sum;
            carry = sum > 0xFFFFFFFF;
            overflow = (~(registers[Rd] ^ registers[Rs]) & (registers[Rd] ^ result)) >> 31;
            break;
        }
        case 0x6: {
            uint32_t c = (cpsr >> 29) & 1;
            uint64_t diff = (uint64_t)registers[Rd] - registers[Rs] - !c;
            result = (uint32_t)diff;
            carry = registers[Rd] >= registers[Rs] + !c;
            overflow = ((registers[Rd] ^ registers[Rs]) & (registers[Rd] ^ result)) >> 31;
            break;
        }
        case 0x7:
            result = shiftValue(registers[Rd], 3, registers[Rs] & 0xFF, carry);
            break;
        case 0x8:
            result = registers[Rd] & registers[Rs];
            setNZ(result);
            return;
        case 0x9: {
            result = 0 - registers[Rs];
            carry = registers[Rs] == 0;
            overflow = (registers[Rs] & result) >> 31;
            break;
        }
        case 0xA: {
            uint64_t diff = (uint64_t)registers[Rd] - registers[Rs];
            result = (uint32_t)diff;
            carry = registers[Rd] >= registers[Rs];
            overflow = ((registers[Rd] ^ registers[Rs]) & (registers[Rd] ^ result)) >> 31;
            setNZCV(result, carry, overflow);
            return;
        }
        case 0xB: {
            uint64_t sum = (uint64_t)registers[Rd] + registers[Rs];
            result = (uint32_t)sum;
            carry = sum > 0xFFFFFFFF;
            overflow = (~(registers[Rd] ^ registers[Rs]) & (registers[Rd] ^ result)) >> 31;
            setNZCV(result, carry, overflow);
            return;
        }
        case 0xC:
            result = registers[Rd] | registers[Rs];
            break;
        case 0xD:
            result = registers[Rd] * registers[Rs];
            break;
        case 0xE:
            result = registers[Rd] & ~registers[Rs];
            break;
        case 0xF:
            result = ~registers[Rs];
            break;
        default:
            result = 0;
    }

    registers[Rd] = result;
    setNZ(result);
    cpsr = (cpsr & ~(1 << 29)) | (carry << 29);
}

void CPU::thumbHiRegisterOps(uint16_t instruction) {
    uint8_t op = (instruction >> 8) & 3;
    bool H1 = (instruction >> 7) & 1;
    bool H2 = (instruction >> 6) & 1;
    uint8_t Rs = ((instruction >> 3) & 7) | (H2 << 3);
    uint8_t Rd = (instruction & 7) | (H1 << 3);

    switch (op) {
        case 0:
            registers[Rd] += registers[Rs];
            break;
        case 1: {
            uint64_t diff = (uint64_t)registers[Rd] - registers[Rs];
            uint32_t result = (uint32_t)diff;
            bool carry = registers[Rd] >= registers[Rs];
            bool overflow = ((registers[Rd] ^ registers[Rs]) & (registers[Rd] ^ result)) >> 31;
            setNZCV(result, carry, overflow);
            return;
        }
        case 2:
            registers[Rd] = registers[Rs];
            break;
        case 3:
            if (registers[Rs] & 1) {
                cpsr |= (1 << 5);
                registers[15] = registers[Rs] & ~1;
            } else {
                cpsr &= ~(1 << 5);
                registers[15] = registers[Rs] & ~3;
            }
            return;
    }

    if (Rd == 15) {
        registers[15] &= ~1;
    }
}

void CPU::thumbPCRelativeLoad(uint16_t instruction) {
    uint8_t Rd = (instruction >> 8) & 7;
    uint8_t imm = instruction & 0xFF;

    uint32_t address = (registers[15] & ~2) + (imm << 2);
    registers[Rd] = mmu.read32(address);
}

void CPU::thumbLoadStoreRegOffset(uint16_t instruction) {
    bool L = (instruction >> 11) & 1;
    bool B = (instruction >> 10) & 1;
    uint8_t Ro = (instruction >> 6) & 7;
    uint8_t Rb = (instruction >> 3) & 7;
    uint8_t Rd = instruction & 7;

    uint32_t address = registers[Rb] + registers[Ro];

    if (L) {
        if (B) {
            registers[Rd] = mmu.read8(address);
        } else {
            registers[Rd] = mmu.read32(address);
        }
    } else {
        if (B) {
            mmu.write8(address, registers[Rd] & 0xFF);
        } else {
            mmu.write32(address, registers[Rd]);
        }
    }
}

void CPU::thumbLoadStoreSignExtend(uint16_t instruction) {
    uint8_t op = (instruction >> 10) & 3;
    uint8_t Ro = (instruction >> 6) & 7;
    uint8_t Rb = (instruction >> 3) & 7;
    uint8_t Rd = instruction & 7;

    uint32_t address = registers[Rb] + registers[Ro];

    switch (op) {
        case 0:
            mmu.write16(address, registers[Rd] & 0xFFFF);
            break;
        case 1: {
            int8_t val = mmu.read8(address);
            registers[Rd] = (uint32_t)(int32_t)val;
            break;
        }
        case 2:
            registers[Rd] = mmu.read16(address);
            break;
        case 3: {
            int16_t val = mmu.read16(address);
            registers[Rd] = (uint32_t)(int32_t)val;
            break;
        }
    }
}

void CPU::thumbLoadStoreImmediate(uint16_t instruction) {
    bool B = (instruction >> 12) & 1;
    bool L = (instruction >> 11) & 1;
    uint8_t offset = (instruction >> 6) & 0x1F;
    uint8_t Rb = (instruction >> 3) & 7;
    uint8_t Rd = instruction & 7;

    uint32_t address;
    if (B) {
        address = registers[Rb] + offset;
    } else {
        address = registers[Rb] + (offset << 2);
    }

    if (L) {
        if (B) {
            registers[Rd] = mmu.read8(address);
        } else {
            registers[Rd] = mmu.read32(address);
        }
    } else {
        if (B) {
            mmu.write8(address, registers[Rd] & 0xFF);
        } else {
            mmu.write32(address, registers[Rd]);
        }
    }
}

void CPU::thumbLoadStoreHalfword(uint16_t instruction) {
    bool L = (instruction >> 11) & 1;
    uint8_t offset = (instruction >> 6) & 0x1F;
    uint8_t Rb = (instruction >> 3) & 7;
    uint8_t Rd = instruction & 7;

    uint32_t address = registers[Rb] + (offset << 1);

    if (L) {
        registers[Rd] = mmu.read16(address);
    } else {
        mmu.write16(address, registers[Rd] & 0xFFFF);
    }
}

void CPU::thumbSPRelativeLoadStore(uint16_t instruction) {
    bool L = (instruction >> 11) & 1;
    uint8_t Rd = (instruction >> 8) & 7;
    uint8_t imm = instruction & 0xFF;

    uint32_t address = registers[13] + (imm << 2);

    if (L) {
        registers[Rd] = mmu.read32(address);
    } else {
        mmu.write32(address, registers[Rd]);
    }
}

void CPU::thumbLoadAddress(uint16_t instruction) {
    bool SP = (instruction >> 11) & 1;
    uint8_t Rd = (instruction >> 8) & 7;
    uint8_t imm = instruction & 0xFF;

    if (SP) {
        registers[Rd] = registers[13] + (imm << 2);
    } else {
        registers[Rd] = (registers[15] & ~2) + (imm << 2);
    }
}

void CPU::thumbAddOffsetToSP(uint16_t instruction) {
    bool S = (instruction >> 7) & 1;
    uint8_t imm = instruction & 0x7F;

    if (S) {
        registers[13] -= imm << 2;
    } else {
        registers[13] += imm << 2;
    }
}

void CPU::thumbPushPop(uint16_t instruction) {
    bool L = (instruction >> 11) & 1;
    bool R = (instruction >> 8) & 1;
    uint8_t regList = instruction & 0xFF;

    if (L) {
        for (int i = 0; i < 8; i++) {
            if (regList & (1 << i)) {
                registers[i] = mmu.read32(registers[13]);
                registers[13] += 4;
            }
        }
        if (R) {
            registers[15] = mmu.read32(registers[13]) & ~1;
            registers[13] += 4;
        }
    } else {
        if (R) {
            registers[13] -= 4;
            mmu.write32(registers[13], registers[14]);
        }
        for (int i = 7; i >= 0; i--) {
            if (regList & (1 << i)) {
                registers[13] -= 4;
                mmu.write32(registers[13], registers[i]);
            }
        }
    }
}

void CPU::thumbMultipleLoadStore(uint16_t instruction) {
    bool L = (instruction >> 11) & 1;
    uint8_t Rb = (instruction >> 8) & 7;
    uint8_t regList = instruction & 0xFF;

    uint32_t address = registers[Rb];

    for (int i = 0; i < 8; i++) {
        if (regList & (1 << i)) {
            if (L) {
                registers[i] = mmu.read32(address);
            } else {
                mmu.write32(address, registers[i]);
            }
            address += 4;
        }
    }

    registers[Rb] = address;
}

void CPU::thumbConditionalBranch(uint16_t instruction) {
    uint8_t cond = (instruction >> 8) & 0xF;
    int8_t offset = instruction & 0xFF;

    bool N = (cpsr >> 31) & 1;
    bool Z = (cpsr >> 30) & 1;
    bool C = (cpsr >> 29) & 1;
    bool V = (cpsr >> 28) & 1;

    bool take = false;
    switch (cond) {
        case 0x0: take = Z; break;
        case 0x1: take = !Z; break;
        case 0x2: take = C; break;
        case 0x3: take = !C; break;
        case 0x4: take = N; break;
        case 0x5: take = !N; break;
        case 0x6: take = V; break;
        case 0x7: take = !V; break;
        case 0x8: take = C && !Z; break;
        case 0x9: take = !C || Z; break;
        case 0xA: take = N == V; break;
        case 0xB: take = N != V; break;
        case 0xC: take = !Z && (N == V); break;
        case 0xD: take = Z || (N != V); break;
    }

    if (take) {
        registers[15] += (int32_t)offset * 2 + 2;
    }
}

void CPU::thumbSoftwareInterrupt(uint16_t instruction) {
    uint8_t comment = instruction & 0xFF;
    handleSWI(comment);
}

void CPU::thumbUnconditionalBranch(uint16_t instruction) {
    int16_t offset = instruction & 0x7FF;
    if (offset & 0x400) {
        offset |= 0xF800;
    }
    registers[15] += offset * 2 + 2;
}

void CPU::thumbLongBranchLink(uint16_t instruction) {
    bool H = (instruction >> 11) & 1;
    uint16_t offset = instruction & 0x7FF;

    if (!H) {
        int32_t off = offset;
        if (off & 0x400) {
            off |= 0xFFFFF800;
        }
        registers[14] = registers[15] + 2 + (off << 12);
    } else {
        uint32_t temp = registers[15];
        registers[15] = (registers[14] + (offset << 1)) & ~1;
        registers[14] = temp | 1;
    }
}

void CPU::setNZ(uint32_t result) {
    cpsr &= ~((1 << 31) | (1 << 30));
    if (result == 0) cpsr |= (1 << 30);
    if (result & 0x80000000) cpsr |= (1 << 31);
}

void CPU::setNZCV(uint32_t result, bool carry, bool overflow) {
    cpsr &= ~((1 << 31) | (1 << 30) | (1 << 29) | (1 << 28));
    if (result == 0) cpsr |= (1 << 30);
    if (result & 0x80000000) cpsr |= (1 << 31);
    if (carry) cpsr |= (1 << 29);
    if (overflow) cpsr |= (1 << 28);
}

uint32_t CPU::shiftValue(uint32_t value, int shiftType, int shiftAmount, bool& carryOut) {
    if (shiftAmount == 0) {
        return value;
    }

    switch (shiftType) {
        case 0:
            if (shiftAmount >= 32) {
                carryOut = (shiftAmount == 32) ? (value & 1) : 0;
                return 0;
            }
            carryOut = (value >> (32 - shiftAmount)) & 1;
            return value << shiftAmount;
        case 1:
            if (shiftAmount >= 32) {
                carryOut = (shiftAmount == 32) ? ((value >> 31) & 1) : 0;
                return 0;
            }
            carryOut = (value >> (shiftAmount - 1)) & 1;
            return value >> shiftAmount;
        case 2:
            if (shiftAmount >= 32) {
                carryOut = (value >> 31) & 1;
                return (value >> 31) ? 0xFFFFFFFF : 0;
            }
            carryOut = (value >> (shiftAmount - 1)) & 1;
            return (int32_t)value >> shiftAmount;
        case 3:
            shiftAmount &= 31;
            if (shiftAmount == 0) {
                return value;
            }
            carryOut = (value >> (shiftAmount - 1)) & 1;
            return rotateRight(value, shiftAmount);
    }
    return value;
}

uint32_t CPU::rotateRight(uint32_t value, int amount) {
    amount &= 31;
    if (amount == 0) return value;
    return (value >> amount) | (value << (32 - amount));
}

CPUMode CPU::getCurrentMode() {
    return static_cast<CPUMode>(cpsr & 0x1F);
}

int CPU::getSPSRIndex() {
    switch (getCurrentMode()) {
        case CPUMode::FIQ: return 0;
        case CPUMode::IRQ: return 1;
        case CPUMode::Supervisor: return 2;
        case CPUMode::Abort: return 3;
        case CPUMode::Undefined: return 4;
        default: return -1;
    }
}

void CPU::switchMode(CPUMode newMode) {
    CPUMode oldMode = getCurrentMode();
    if (oldMode == newMode) return;
    
    bool oldIsFIQ = (oldMode == CPUMode::FIQ);
    bool newIsFIQ = (newMode == CPUMode::FIQ);

    if (oldIsFIQ) {
        for (int i = 0; i < 7; i++) bankedFIQ[i] = registers[8 + i];
        for (int i = 0; i < 7; i++) registers[8 + i] = bankedUSR[i];
    } else if (newIsFIQ) {
        for (int i = 0; i < 7; i++) bankedUSR[i] = registers[8 + i];
        for (int i = 0; i < 7; i++) registers[8 + i] = bankedFIQ[i];
    }

    if (!oldIsFIQ && oldMode != CPUMode::User && oldMode != CPUMode::System) {
        switch (oldMode) {
            case CPUMode::IRQ:
                bankedIRQ[0] = registers[13];
                bankedIRQ[1] = registers[14];
                break;
            case CPUMode::Supervisor:
                bankedSVC[0] = registers[13];
                bankedSVC[1] = registers[14];
                break;
            case CPUMode::Abort:
                bankedABT[0] = registers[13];
                bankedABT[1] = registers[14];
                break;
            case CPUMode::Undefined:
                bankedUND[0] = registers[13];
                bankedUND[1] = registers[14];
                break;
            default:
                break;
        }
    }

    if (!newIsFIQ && newMode != CPUMode::User && newMode != CPUMode::System) {
        switch (newMode) {
            case CPUMode::IRQ:
                registers[13] = bankedIRQ[0];
                registers[14] = bankedIRQ[1];
                break;
            case CPUMode::Supervisor:
                registers[13] = bankedSVC[0];
                registers[14] = bankedSVC[1];
                break;
            case CPUMode::Abort:
                registers[13] = bankedABT[0];
                registers[14] = bankedABT[1];
                break;
            case CPUMode::Undefined:
                registers[13] = bankedUND[0];
                registers[14] = bankedUND[1];
                break;
            default:
                break;
        }
    }

    cpsr = (cpsr & ~0x1F) | static_cast<uint32_t>(newMode);
}
