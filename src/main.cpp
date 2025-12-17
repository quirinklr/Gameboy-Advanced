#include <SDL.h>
#include <iostream>
#include <string>
#include <cstring>
#include "GBA.h"
#include "PPU.h"

bool checkTestResult(const uint32_t* framebuffer) {
    int passedTextPixels = 0;
    int failedTextPixels = 0;
    
    for (int y = 76; y < 84; y++) {
        for (int x = 56; x < 64; x++) {
            uint32_t pixel = framebuffer[y * SCREEN_WIDTH + x];
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t b = pixel & 0xFF;
            if (r < 50 && g < 50 && b < 50) passedTextPixels++;
        }
        
        for (int x = 60; x < 68; x++) {
            uint32_t pixel = framebuffer[y * SCREEN_WIDTH + x];
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t b = pixel & 0xFF;
            if (r < 50 && g < 50 && b < 50) failedTextPixels++;
        }
    }
    
    if (passedTextPixels > 20 && passedTextPixels > failedTextPixels) return true;
    if (failedTextPixels > 20) return false;
    
    return passedTextPixels >= failedTextPixels;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <rom.gba> [--test]" << std::endl;
        return 1;
    }

    bool testMode = false;
    std::string romPath;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0) {
            testMode = true;
        } else {
            romPath = argv[i];
        }
    }

    if (romPath.empty()) {
        std::cerr << "No ROM file specified" << std::endl;
        return 1;
    }

    Uint32 initFlags = testMode ? SDL_INIT_VIDEO : SDL_INIT_VIDEO;
    if (SDL_Init(initFlags) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::string title = "GBA Emulator - " + romPath.substr(romPath.find_last_of("/\\") + 1);

    SDL_Window* window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * 3,
        SCREEN_HEIGHT * 3,
        testMode ? SDL_WINDOW_HIDDEN : (SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE)
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    if (!texture) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    GBA gba;
    if (!gba.loadROM(romPath)) {
        std::cerr << "Failed to load ROM: " << romPath << std::endl;
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    Uint32 frameStart;
    int frameCount = 0;
    Uint32 fpsTimer = SDL_GetTicks();
    int testFrames = 0;
    const int TEST_FRAME_LIMIT = 120;

    while (running) {
        frameStart = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                bool pressed = (event.type == SDL_KEYDOWN);
                int key = -1;

                switch (event.key.keysym.sym) {
                    case SDLK_z: key = 0; break;
                    case SDLK_x: key = 1; break;
                    case SDLK_BACKSPACE: key = 2; break;
                    case SDLK_RETURN: key = 3; break;
                    case SDLK_RIGHT: key = 4; break;
                    case SDLK_LEFT: key = 5; break;
                    case SDLK_UP: key = 6; break;
                    case SDLK_DOWN: key = 7; break;
                    case SDLK_a: key = 8; break;
                    case SDLK_s: key = 9; break;
                    case SDLK_ESCAPE: running = false; break;
                }

                if (key != -1) {
                    gba.updateKey(key, pressed);
                }
            }
        }

        gba.runFrame();
        
        static int debugFrames = 0;
        if (++debugFrames >= 180 && !testMode) {
            debugFrames = 0;
            const uint32_t* fb = gba.getFramebuffer();
            int nonBlackPixels = 0;
            for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
                if (fb[i] != 0xFF000000 && fb[i] != 0) nonBlackPixels++;
            }
            std::cout << "Non-black pixels: " << nonBlackPixels << std::endl;
        }

        SDL_UpdateTexture(texture, nullptr, gba.getFramebuffer(), SCREEN_WIDTH * sizeof(uint32_t));

        if (!testMode) {
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }

        frameCount++;
        testFrames++;

        if (testMode && testFrames >= TEST_FRAME_LIMIT) {
            bool passed = checkTestResult(gba.getFramebuffer());
            std::cout << (passed ? "PASSED" : "FAILED") << std::endl;
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return passed ? 0 : 1;
        }

        if (!testMode) {
            Uint32 elapsed = SDL_GetTicks() - fpsTimer;
            if (elapsed >= 1000) {
                float fps = frameCount * 1000.0f / elapsed;
                char newTitle[256];
                snprintf(newTitle, sizeof(newTitle), "GBA Emulator - %.1f fps", fps);
                SDL_SetWindowTitle(window, newTitle);
                frameCount = 0;
                fpsTimer = SDL_GetTicks();
            }
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
