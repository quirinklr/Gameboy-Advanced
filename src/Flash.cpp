#include "Flash.h"

Flash::Flash(FlashSize size) : flashSize(size) {
    reset();
}

void Flash::reset() {
    memory.fill(0xFF);
    state = FlashState::Ready;
    currentBank = 0;
    chipIdMode = false;
}

void Flash::setSize(FlashSize size) {
    flashSize = size;
}

uint8_t Flash::read(uint32_t address) {
    address &= 0xFFFF;
    
    if (chipIdMode) {
        if (address == 0) return MANUFACTURER_ID;
        if (address == 1) return (flashSize == FlashSize::Flash128K) ? DEVICE_ID_128K : DEVICE_ID_64K;
    }
    
    uint32_t fullAddr = address;
    if (flashSize == FlashSize::Flash128K) {
        fullAddr += currentBank * 0x10000;
    }
    
    if (fullAddr < memory.size()) {
        return memory[fullAddr];
    }
    return 0xFF;
}

void Flash::write(uint32_t address, uint8_t value) {
    address &= 0xFFFF;
    
    switch (state) {
        case FlashState::Ready:
            if (address == 0x5555 && value == 0xAA) {
                state = FlashState::Command1;
            }
            break;
            
        case FlashState::Command1:
            if (address == 0x2AAA && value == 0x55) {
                state = FlashState::Command2;
            } else {
                state = FlashState::Ready;
            }
            break;
            
        case FlashState::Command2:
            if (address == 0x5555) {
                handleCommand(address, value);
            } else {
                state = FlashState::Ready;
            }
            break;
            
        case FlashState::Erase1:
            if (address == 0x5555 && value == 0xAA) {
                state = FlashState::Erase2;
            } else {
                state = FlashState::Ready;
            }
            break;
            
        case FlashState::Erase2:
            if (address == 0x2AAA && value == 0x55) {
                state = FlashState::Erase3;
            } else {
                state = FlashState::Ready;
            }
            break;
            
        case FlashState::Erase3:
            if (value == 0x10) {
                eraseChip();
            } else if (value == 0x30) {
                uint32_t sector = address / 0x1000;
                if (flashSize == FlashSize::Flash128K) {
                    sector += currentBank * 16;
                }
                eraseSector(sector);
            }
            state = FlashState::Ready;
            break;
            
        case FlashState::Program: {
            uint32_t fullAddr = address;
            if (flashSize == FlashSize::Flash128K) {
                fullAddr += currentBank * 0x10000;
            }
            if (fullAddr < memory.size()) {
                memory[fullAddr] &= value;
            }
            state = FlashState::Ready;
            break;
        }
            
        case FlashState::BankSelect:
            if (address == 0x0000) {
                currentBank = value & 1;
            }
            state = FlashState::Ready;
            break;
            
        case FlashState::ChipID:
            if (value == 0xF0) {
                chipIdMode = false;
                state = FlashState::Ready;
            }
            break;
            
        default:
            state = FlashState::Ready;
            break;
    }
}

void Flash::handleCommand(uint32_t address, uint8_t value) {
    switch (value) {
        case 0x90:
            chipIdMode = true;
            state = FlashState::ChipID;
            break;
        case 0xF0:
            chipIdMode = false;
            state = FlashState::Ready;
            break;
        case 0x80:
            state = FlashState::Erase1;
            break;
        case 0xA0:
            state = FlashState::Program;
            break;
        case 0xB0:
            if (flashSize == FlashSize::Flash128K) {
                state = FlashState::BankSelect;
            } else {
                state = FlashState::Ready;
            }
            break;
        default:
            state = FlashState::Ready;
            break;
    }
}

void Flash::eraseChip() {
    size_t eraseSize = (flashSize == FlashSize::Flash128K) ? 0x20000 : 0x10000;
    for (size_t i = 0; i < eraseSize; i++) {
        memory[i] = 0xFF;
    }
}

void Flash::eraseSector(uint32_t sector) {
    uint32_t start = sector * 0x1000;
    uint32_t end = start + 0x1000;
    
    if (end > memory.size()) end = memory.size();
    
    for (uint32_t i = start; i < end; i++) {
        memory[i] = 0xFF;
    }
}
