#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

#include "GBA.h"
#include "Bitmap.h"

namespace fs = std::filesystem;

Bitmap runTest(const std::string& romPath, int frames) {
    GBA gba;
    if (!gba.loadROM(romPath)) {
        std::cerr << "Failed to load ROM: " << romPath << std::endl;
        return {};
    }

    for (int i = 0; i < frames; ++i) {
        gba.runFrame();
    }
    
    const uint32_t* fb = gba.getFramebuffer();
    Bitmap bmp;
    bmp.width = 240;
    bmp.height = 160;
    bmp.pixels.assign(fb, fb + (240 * 160));
    return bmp; 
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: GBA_PPU_Tests <test_dir>" << std::endl;
        return 1;
    }

    std::string testDir = argv[1];
    int passed = 0;
    int total = 0;

    std::cout << "Running PPU Tests in: " << testDir << std::endl;

    for (const auto& entry : fs::directory_iterator(testDir)) {
        if (entry.path().extension() == ".gba") {
            total++;
            std::string romPath = entry.path().string();
            
            fs::path expectedP = entry.path();
            expectedP.replace_extension(".bmp");
            std::string expectedPath = expectedP.string();

            fs::path filename = entry.path().filename();
            filename.replace_extension(".bmp");
            std::string outputPath = (entry.path().parent_path() / ("output_" + filename.string())).string();

            std::cout << "Testing " << entry.path().filename() << "... ";

            Bitmap result = runTest(romPath, 5);
            
            if (fs::exists(expectedPath)) {
                Bitmap expected = Bitmap::load(expectedPath);
                if (result.pixels == expected.pixels) {
                    std::cout << "PASSED" << std::endl;
                    passed++;
                } else {
                    std::cout << "FAILED (Mismatch)" << std::endl;
                    result.save(outputPath);
                }
            } else {
                std::cout << "GENERATED (No reference)" << std::endl;
                result.save(expectedPath);
                passed++;
            }
        }
    }

    std::cout << "Results: " << passed << "/" << total << " passed." << std::endl;
    return (passed == total) ? 0 : 1;
}
