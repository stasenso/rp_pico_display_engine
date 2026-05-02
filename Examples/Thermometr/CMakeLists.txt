cmake_minimum_required(VERSION 3.18.4)

# EN: Change this line to switch the target board manually: `pico` for RP2040 or `pico2` for RP2350.
# RU: Измените эту строку, чтобы вручную переключить целевую плату: `pico` для RP2040 или `pico2` для RP2350.
set(PICO_BOARD pico2 CACHE STRING "Pico SDK target board")

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(thermometr_example C CXX ASM)
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

# EN: Change the SPI bus, GPIO pins, and display controller here for this example.
# RU: Здесь изменяются SPI-шина, GPIO-пины и контроллер дисплея для этого примера.
set(DISPLAY_SPI_PORT spi1)
set(DISPLAY_PIN_MOSI 15) # DIN / SDA
set(DISPLAY_PIN_SCK 14)  # CLK / SCL / SCK
set(DISPLAY_PIN_CS 13)   # CS / SS
set(DISPLAY_PIN_DC 12)   # DC / A0 / RS
set(DISPLAY_PIN_RST 11)  # RST / RES / RESET
set(DISPLAY_PIN_BL 10)   # BL / LED / LEDK
# EN: Available values: DISPLAY_TYPE_ST7789, DISPLAY_TYPE_ILI9341.
# RU: Доступные значения: DISPLAY_TYPE_ST7789, DISPLAY_TYPE_ILI9341.
set(DISPLAY_TYPE "DISPLAY_TYPE_ILI9341" CACHE STRING "Display controller backend")
set_property(CACHE DISPLAY_TYPE PROPERTY STRINGS DISPLAY_TYPE_ST7789 DISPLAY_TYPE_ILI9341)

# EN: These definitions forward the selected board wiring into the display engine without editing its sources.
# RU: Эти определения передают выбранную разводку платы в display engine без правки его исходников.
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
    src/dht22.c
)

pico_generate_pio_header(${PROJECT_NAME}
    ${CMAKE_CURRENT_LIST_DIR}/src/dht22.pio
)

target_link_libraries(${PROJECT_NAME}
    display_engine
    hardware_pio
)

pico_add_extra_outputs(${PROJECT_NAME})
