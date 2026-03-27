cmake_minimum_required(VERSION 3.18.4)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(thermometr_example C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json" FORCE)

pico_sdk_init()

add_executable(${PROJECT_NAME}
    src/main.c
    ../../src/Font/font_data.c
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

# Display pin mapping (single source of truth for this example).
set(DISPLAY_SPI_PORT spi1)
set(DISPLAY_PIN_MOSI 15)
set(DISPLAY_PIN_SCK 14)
set(DISPLAY_PIN_CS 13)
set(DISPLAY_PIN_DC 12)
set(DISPLAY_PIN_RST 11)
set(DISPLAY_PIN_BL 10)

# Pass board pin mapping into display.c without editing engine sources.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SPI_PORT=${DISPLAY_SPI_PORT}
    PIN_MOSI=${DISPLAY_PIN_MOSI}
    PIN_SCK=${DISPLAY_PIN_SCK}
    PIN_CS=${DISPLAY_PIN_CS}
    PIN_DC=${DISPLAY_PIN_DC}
    PIN_RST=${DISPLAY_PIN_RST}
    PIN_BL=${DISPLAY_PIN_BL}
)

pico_add_extra_outputs(${PROJECT_NAME})
