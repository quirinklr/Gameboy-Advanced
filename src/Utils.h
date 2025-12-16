#pragma once

#include <cstdint>

namespace Utils {

template <typename T>
inline T getBit(T value, int bit) {
    return (value >> bit) & 1;
}

template <typename T>
inline T getBits(T value, int start, int count) {
    return (value >> start) & ((1 << count) - 1);
}

template <typename T>
inline T setBit(T value, int bit, bool set) {
    if (set) {
        return value | (1 << bit);
    }
    return value & ~(1 << bit);
}

inline uint16_t read16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

inline uint32_t read32(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

inline void write16(uint8_t* data, uint16_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
}

inline void write32(uint8_t* data, uint32_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
}

}
