cmake_minimum_required(VERSION 3.18.4)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(thermometr_example C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json" FORCE)

pico_sdk_init()

add_executable(${PROJECT_NAME}
    src/main.c
    ../../src/core/display.c
    ../../src/render/context.c
    ../../src/render/line.c
    ../../src/render/grid.c
    ../../src/render/sine_wave.c
    ../../src/render/bezier.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ../../include
    ../../include/display
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_spi
    hardware_dma
    hardware_timer
    pico_multicore
)

pico_add_extra_outputs(${PROJECT_NAME})
