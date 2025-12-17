#pragma once

#include <cstdint>
#include <array>

enum class FlashState {
    Ready,
    Command1,
    Command2,
    ChipID,
    Erase1,
    Erase2,
    Erase3,
    Program,
    BankSelect
};

enum class FlashSize {
    Flash64K,
    Flash128K
};

class Flash {
public:
    Flash(FlashSize size = FlashSize::Flash64K);
    
    void reset();
    
    uint8_t read(uint32_t address);
    void write(uint32_t address, uint8_t value);
    
    void setSize(FlashSize size);
    
private:
    void handleCommand(uint32_t address, uint8_t value);
    void eraseChip();
    void eraseSector(uint32_t sector);
    
    FlashState state = FlashState::Ready;
    FlashSize flashSize = FlashSize::Flash64K;
    
    uint8_t currentBank = 0;
    bool chipIdMode = false;
    
    std::array<uint8_t, 0x20000> memory{};
    
    static constexpr uint16_t MANUFACTURER_ID = 0x32;
    static constexpr uint16_t DEVICE_ID_64K = 0x1B;
    static constexpr uint16_t DEVICE_ID_128K = 0x09;
};
