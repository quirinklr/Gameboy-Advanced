cmake_minimum_required(VERSION 3.20)
project(GBA_Emulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.28.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL2)

set(SOURCES
    src/main.cpp
    src/GBA.cpp
    src/CPU.cpp
    src/MMU.cpp
    src/PPU.cpp
    src/Flash.cpp
)

set(HEADERS
    src/GBA.h
    src/CPU.h
    src/MMU.h
    src/PPU.h
    src/Utils.h
    src/Flash.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main)

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()

set(TEST_SOURCES
    tests/test_runner.cpp
    src/GBA.cpp
    src/CPU.cpp
    src/MMU.cpp
    src/PPU.cpp
    src/Flash.cpp
)

add_executable(GBA_Tests ${TEST_SOURCES})
target_include_directories(GBA_Tests PRIVATE src)
target_compile_definitions(GBA_Tests PRIVATE HEADLESS_TEST)

add_executable(GBA_PPU_Tests 
    tests/test_ppu_runner.cpp
    src/GBA.cpp
    src/CPU.cpp
    src/MMU.cpp
    src/PPU.cpp
    src/Flash.cpp
)
target_include_directories(GBA_PPU_Tests PRIVATE src tests)
target_compile_definitions(GBA_PPU_Tests PRIVATE HEADLESS_TEST)
