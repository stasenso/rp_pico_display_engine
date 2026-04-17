cmake_minimum_required(VERSION 3.18.4)

set(TARGET_BOARD "pico2_RP2350" CACHE STRING "Target board")
set_property(CACHE TARGET_BOARD PROPERTY STRINGS pico_RP2040 pico2_RP2350)

if(TARGET_BOARD STREQUAL "pico_RP2040")
    set(PICO_BOARD pico CACHE STRING "Pico SDK target board" FORCE)
elseif(TARGET_BOARD STREQUAL "pico2_RP2350")
    set(PICO_BOARD pico2 CACHE STRING "Pico SDK target board" FORCE)
endif()

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(engine_demo_example C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json" FORCE)

pico_sdk_init()

add_library(display_engine STATIC
    ../../src/Font/font_data.c
    ../../src/core/display.c
    ../../src/core/display_transport.c
    ../../src/core/display_driver.c
    ../../src/render/context.c
    ../../src/render/line.c
    ../../src/render/grid.c
    ../../src/render/sine_wave.c
    ../../src/render/bezier.c
)

target_include_directories(display_engine PUBLIC
    ../../include
    ../../include/display
)

target_link_libraries(display_engine PUBLIC
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
# Available values: DISPLAY_TYPE_ST7789, DISPLAY_TYPE_ILI9341
set(DISPLAY_TYPE "DISPLAY_TYPE_ST7789" CACHE STRING "Display controller backend")
set_property(CACHE DISPLAY_TYPE PROPERTY STRINGS DISPLAY_TYPE_ST7789 DISPLAY_TYPE_ILI9341)

# Pass display mapping into display backend without editing engine sources.
target_compile_definitions(display_engine PUBLIC
    DISPLAY_TYPE=${DISPLAY_TYPE}
    DISPLAY_SPI_PORT=${DISPLAY_SPI_PORT}
    DISPLAY_PIN_MOSI=${DISPLAY_PIN_MOSI}
    DISPLAY_PIN_SCK=${DISPLAY_PIN_SCK}
    DISPLAY_PIN_CS=${DISPLAY_PIN_CS}
    DISPLAY_PIN_DC=${DISPLAY_PIN_DC}
    DISPLAY_PIN_RST=${DISPLAY_PIN_RST}
    DISPLAY_PIN_BL=${DISPLAY_PIN_BL}
)

add_executable(${PROJECT_NAME}
    src/main.c
)

target_link_libraries(${PROJECT_NAME}
    display_engine
)

pico_add_extra_outputs(${PROJECT_NAME})
