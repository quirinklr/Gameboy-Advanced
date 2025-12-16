# GBA Emulator Project (C++20 / SDL2)

A high-performance Game Boy Advance emulator written in modern C++ (C++20), utilizing SDL2 for rendering and input. This project implements a cycle-accurate ARM7TDMI CPU core, supporting both ARM and Thumb instruction sets, a functional PPU for Mode 3/4/5 rendering, and an MMU with mirroring support.

## Features

### ARM7TDMI Core
- **Full Instruction Set Support**:
  - Implements all **ARM v4T** instructions (Data Processing, Multiply/Long Multiply, LDR/STR, LDM/STM, Swap, Branches, PSR Transfer).
  - Implements all **Thumb** instructions (Formats 1-19).
- **Accurate Pipeline & Timing**:
  - Simulates the 3-stage pipeline (Fetch-Decode-Execute) behavior (PC = Instruction Address + 8/4).
  - Cycle counting for accurate timing emulation.
- **Register Banking**:
  - Complete implementation of banked registers for all CPU modes (User, System, FIQ, IRQ, Supervisor, Abort, Undefined).
- **Verified Accuracy**:
  - Passed **100% of tests** in `gba-tests/arm/arm.gba` (532 tests).
  - Passed **100% of tests** in `gba-tests/thumb/thumb.gba`.

### Memory Management Unit (MMU)
- **Memory Map Implementation**:
  - BIOS (0x00)
  - EWRAM (0x02)
  - IWRAM (0x03)
  - I/O Registers (0x04)
  - Palette RAM (0x05)
  - VRAM (0x06) with correct mirroring behavior.
  - OAM (0x07)
  - Game Pak ROM (0x08 - 0x0D)
  - SRAM (0x0E)
- **Wait State Handling**:
  - Region-specific cycle costs (e.g., fast IWRAM vs slow ROM).

### Picture Processing Unit (PPU)
- **Bitmap Modes Supported**:
  - **Mode 3**: 240x160 Direct Color (15-bit).
  - **Mode 4**: 240x160 Palette Index (8-bit) with Page Flipping.
  - **Mode 5**: 160x128 Direct Color (15-bit) with Page Flipping.
- **Rendering Pipeline**:
  - Scanline-based rendering (HDRAW/HBLANK/VDRAW/VBLANK timings).
  - Accurate VCOUNT and DISPSTAT status updates.

## Build Instructions

### Prerequisites
- **CMake** (3.10+)
- **C++ Compiler** with C++20 support (MSVC, GCC, Clang).
- **SDL2** (Fetched automatically via CMake FetchContent).

### Building
1. Clone the repository.
2. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```
3. Configure and build:
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release
   cmake --build . --config Release
   ```

## Running the Emulator

Run the executable with a path to a GBA ROM file:

```bash
./Release/GBA_Emulator.exe path/to/rom.gba
```

### Running Tests
The project includes a headless test runner for verifying CPU correctness against `gba-tests`.

```bash
./Release/GBA_Tests.exe path/to/test_rom.gba
```

## Architecture

- **`src/`**: Source code files.
  - `GBA.cpp/h`: System coordinator (Top-level class).
  - `CPU.cpp/h`: ARM7TDMI implementation (Registers, Decoder, ALU).
  - `MMU.cpp/h`: Memory map, read/write logic, DMA hooks.
  - `PPU.cpp/h`: Graphics rendering engine.
  - `Utils.h`: Common bit-twiddling and helper functions.
  - `main.cpp`: SDL2 entry point, event loop, and frame timing.
- **`tests/`**: Test suite integration.

## License
This project is open-source.
