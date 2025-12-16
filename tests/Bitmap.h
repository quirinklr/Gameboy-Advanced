#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <cstdint>
#include <iostream>

class Bitmap {
public:
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;

    bool save(const std::string& path) const {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;

        uint32_t fileSize = 14 + 40 + (width * height * 4);
        uint32_t dataOffset = 14 + 40;
        
        writeU16(f, 0x4D42);
        writeU32(f, fileSize);
        writeU16(f, 0);
        writeU16(f, 0);
        writeU32(f, dataOffset);

        writeU32(f, 40);
        writeU32(f, width);
        writeU32(f, -height);
        writeU16(f, 1);
        writeU16(f, 32);
        writeU32(f, 0);
        writeU32(f, width * height * 4);
        writeU32(f, 0);
        writeU32(f, 0);
        writeU32(f, 0);
        writeU32(f, 0);

        for (const auto& p : pixels) {
             uint8_t r = (p >> 16) & 0xFF;
             writeU32(f, p);
        }

        return true;
    }

    static Bitmap load(const std::string& path) {
        Bitmap bmp;
        std::ifstream f(path, std::ios::binary);
        if (!f) return bmp;

        f.seekg(14);
        
        uint32_t headerSize = readU32(f);
        int32_t w = readS32(f);
        int32_t h = readS32(f);
        f.seekg(2, std::ios::cur);
        uint16_t bpp = readU16(f);

        if (bpp != 32) {
            std::cerr << "Only 32-bit BMP supported for testing." << std::endl;
            return bmp;
        }

        bmp.width = w;
        bmp.height = std::abs(h);
        bmp.pixels.resize(bmp.width * bmp.height);

        f.seekg(14 + headerSize);

        f.seekg(10);
        uint32_t offset = readU32(f);
        f.seekg(offset);

        for (size_t i = 0; i < bmp.pixels.size(); ++i) {
            bmp.pixels[i] = readU32(f);
        }

        return bmp;
    }

private:
    static void writeU16(std::ofstream& f, uint16_t v) { f.write((const char*)&v, 2); }
    static void writeU32(std::ofstream& f, uint32_t v) { f.write((const char*)&v, 4); }
    static uint16_t readU16(std::ifstream& f) { uint16_t v; f.read((char*)&v, 2); return v; }
    static uint32_t readU32(std::ifstream& f) { uint32_t v; f.read((char*)&v, 4); return v; }
    static int32_t readS32(std::ifstream& f) { int32_t v; f.read((char*)&v, 4); return v; }
};
