# RP Pico Display Engine

A lightweight C display engine for `RP2040/RP2350` and `ST7789` / `ILI9341` SPI displays (DMA) with an explicit `begin/end paint` frame contract.

## Features

- Two modes: `DISPLAY_MODE_SAFE` and `DISPLAY_MODE_RAW`
- 1 or 2 frame buffers (`buffer_count`)
- Non-blocking and blocking frame acquisition:
  - `display_begin_paint_try()`
  - `display_begin_paint_blocking()`
- Safe frame completion via `display_end_paint()`
- Rendering primitives and a font module with Cyrillic support

## Repository Structure

- `include/display/` - public headers for the engine and renderer
- `src/core/display.c` - display engine core
- `src/render/` - primitives implementation
- `include/Font/`, `src/Font/` - font data and text rendering
- `Examples/Thermometr/` - working Pico SDK example project

## Required Environment

- `git`
- `cmake` version `3.18.4+`
- build tool supported by CMake (`make` by default on Unix-like systems)
- `Pico SDK` with `PICO_SDK_PATH` pointing to it
- ARM toolchain `arm-none-eabi-gcc`

## Supported Displays and Default Wiring

Supported controller backends:

- `DISPLAY_TYPE_ST7789`
- `DISPLAY_TYPE_ILI9341`

If you do not pass any display-related compile definitions, the library defaults from [src/core/display_driver.h](/home/smikhai/repo/rp_pico_display_engine/src/core/display_driver.h:1) are used:

- Controller: `DISPLAY_TYPE_ST7789`
- SPI port: `spi0`
- `MOSI=19`
- `SCK=18`
- `CS=17`
- `DC=22`
- `RST=13`
- `BL=12`

These defaults mean: if you build without overriding pins or `DISPLAY_TYPE`, the firmware is meant for an `ST7789` display wired to those RP2040/RP2350 GPIOs.

## Quick Start

### 1. Add as a subproject (recommended)

The most practical integration pattern is to add this repository (for example, as a git submodule) and build `display_engine` from your main `CMakeLists.txt`.
You do not have to include all rendering primitives immediately: keep only what you need and uncomment extra modules later.

Create a new project and add this repository as a submodule:

```bash
mkdir pico_display_app
cd pico_display_app
git init
git submodule add https://github.com/stasenso/rp_pico_display_engine.git external/rp_pico_display_engine
git submodule update --init --recursive
mkdir -p src
```

Make sure `PICO_SDK_PATH` is set:

```bash
export PICO_SDK_PATH=/absolute/path/to/pico-sdk
```

Create the root `CMakeLists.txt`:

```bash
cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.18.4)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(my_app C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(DISPLAY_ENGINE_DIR ${CMAKE_CURRENT_LIST_DIR}/external/rp_pico_display_engine)

add_library(display_engine STATIC
    ${DISPLAY_ENGINE_DIR}/src/Font/font_data.c
    ${DISPLAY_ENGINE_DIR}/src/core/display.c
    ${DISPLAY_ENGINE_DIR}/src/core/display_transport.c
    ${DISPLAY_ENGINE_DIR}/src/core/display_driver.c
    ${DISPLAY_ENGINE_DIR}/src/render/context.c
    ${DISPLAY_ENGINE_DIR}/src/render/line.c
    # Uncomment if/when you need these primitives:
    # ${DISPLAY_ENGINE_DIR}/src/render/grid.c
    # ${DISPLAY_ENGINE_DIR}/src/render/sine_wave.c
    # ${DISPLAY_ENGINE_DIR}/src/render/bezier.c
)

target_include_directories(display_engine PUBLIC
    ${DISPLAY_ENGINE_DIR}/include
    ${DISPLAY_ENGINE_DIR}/include/display
)

target_link_libraries(display_engine PUBLIC
    pico_stdlib
    hardware_spi
    hardware_dma
    hardware_timer
    pico_multicore
)

target_compile_definitions(display_engine PUBLIC
    DISPLAY_TYPE=DISPLAY_TYPE_ST7789
    DISPLAY_SPI_PORT=spi1
    DISPLAY_PIN_MOSI=15
    DISPLAY_PIN_SCK=14
    DISPLAY_PIN_CS=13
    DISPLAY_PIN_DC=12
    DISPLAY_PIN_RST=11
    DISPLAY_PIN_BL=10
)

add_executable(my_app src/main.c)
target_link_libraries(my_app PRIVATE display_engine)
pico_add_extra_outputs(my_app)
EOF
```

Create `src/main.c`:

```bash
cat > src/main.c <<'EOF'
#include "pico/stdlib.h"
#include "display/display.h"
#include "display/render/context.h"

int main(void) {
    stdio_init_all();

    display_config_t cfg = {
        .width = 320,
        .height = 240,
        .buffer_count = 1,
        .mode = DISPLAY_MODE_SAFE
    };
    display_init(&cfg);

    render_ctx_t rc;

    while (1) {
        uint16_t *buf = display_begin_paint_blocking();
        render_begin(&rc, buf, 320, 240);
        render_clear(&rc, RGB16(0, 0, 0));
        display_end_paint();
        sleep_ms(16);
    }
}
EOF
```

Build the project:

```bash
cmake -S . -B build
cmake --build build
```

### 2. Change display pins and controller type

Set these compile definitions on `display_engine`:

- `DISPLAY_SPI_PORT` (`spi0` or `spi1`)
- `DISPLAY_PIN_MOSI`, `DISPLAY_PIN_SCK`, `DISPLAY_PIN_CS`, `DISPLAY_PIN_DC`, `DISPLAY_PIN_RST`, `DISPLAY_PIN_BL`
- `DISPLAY_TYPE` (`DISPLAY_TYPE_ST7789` or `DISPLAY_TYPE_ILI9341`)

If no definitions are provided, these defaults from [src/core/display_driver.h](/home/smikhai/repo/rp_pico_display_engine/src/core/display_driver.h:1) are used:

- `DISPLAY_TYPE=DISPLAY_TYPE_ST7789`
- `DISPLAY_SPI_PORT=spi0`
- `DISPLAY_PIN_MOSI=19`
- `DISPLAY_PIN_SCK=18`
- `DISPLAY_PIN_CS=17`
- `DISPLAY_PIN_DC=22`
- `DISPLAY_PIN_RST=13`
- `DISPLAY_PIN_BL=12`

### 3. Safe mode with a single display loop

Good for a simple loop where dropping a frame is acceptable when the buffer is busy.

```c
#include "display/display.h"
#include "display/render/context.h"

#define WIDTH  320
#define HEIGHT 240

void app_loop(void) {
    display_config_t cfg = {
        .width = WIDTH,
        .height = HEIGHT,
        .buffer_count = 1,
        .mode = DISPLAY_MODE_SAFE
    };
    display_init(&cfg);

    render_ctx_t rc;

    while (1) {
        uint16_t *buf = display_begin_paint_try();
        if (!buf) {
            continue;
        }

        render_begin(&rc, buf, WIDTH, HEIGHT);
        render_clear(&rc, RGB16(0, 0, 0));
        // draw...

        bool ok = display_end_paint();
        hard_assert(ok);
    }
}
```

### 4. Safe mode with two buffers and deferred submit support

In `SAFE + buffer_count = 2`, DMA can scan out one buffer while you render the other one.  
If DMA is busy at `display_end_paint()`, the frame may be deferred (single pending-frame queue).

```c
#include "display/display.h"
#include "display/render/context.h"

#define WIDTH  320
#define HEIGHT 240

void app_loop(void) {
    display_config_t cfg = {
        .width = WIDTH,
        .height = HEIGHT,
        .buffer_count = 2,
        .mode = DISPLAY_MODE_SAFE
    };
    display_init(&cfg);

    render_ctx_t rc;

    while (1) {
        uint16_t *buf = display_begin_paint_try();
        if (!buf) {
            // DMA/queue busy: run other app logic
            continue;
        }

        render_begin(&rc, buf, WIDTH, HEIGHT);
        render_clear(&rc, RGB16(8, 16, 8));
        // draw...

        bool accepted = display_end_paint();
        hard_assert(accepted); // false only on contract misuse
    }
}
```

### 5. Primitive and font headers + output example

Minimal include set:

```c
#include "display/display.h"
#include "display/render/context.h"
#include "display/render/line.h"
#include "Font/font_data.h"            // draw_string(), draw_char(), get_char_width()
```

Optional includes (uncomment when needed):

```c
// #include "display/render/grid.h"
// #include "display/render/sine_wave.h"
// #include "display/render/bezier.h"
```

Primitive + text render example:

```c
render_ctx_t rc;
render_begin(&rc, buf, 320, 240);

render_clear(&rc, RGB16(9, 19, 9));
render_line(&rc, 0, 0, 319, 239, RGB16(255, 0, 0));
// render_grid(&rc, 20, 20, 40, RGB16(12, 26, 13)); // uncomment if you need grid
// render_sine_wave(&rc, 960, 100, 2.0f, 0, 120, phase, RGB16(0, 255, 0)); // uncomment if needed

int px[4] = {20, 80, 140, 220};
int py[4] = {200, 120, 220, 160};
// render_bezier(&rc, px, py, 4, RGB16(255, 255, 0)); // uncomment if needed

draw_string(&rc, 16, 16, L"Cyrillic check", RGB565(255, 255, 255));
```

## Build Example (Pico SDK)

If you just want to build the ready-made example from this repository:

```bash
cd Examples/Thermometr
mkdir -p build && cd build
cmake ..
cmake --build .
```

This example sets the target board directly in [Examples/Thermometr/CMakeLists.txt](/home/smikhai/repo/rp_pico_display_engine/Examples/Thermometr/CMakeLists.txt:1). To switch boards, change:

```cmake
set(PICO_BOARD pico2 CACHE STRING "Pico SDK target board")
```

Use `pico` for RP2040-based Pico or `pico2` for RP2350-based Pico 2. The display configuration in the same file is:

- Controller: `DISPLAY_TYPE_ILI9341`
- SPI port: `spi1`
- `MOSI=15`
- `SCK=14`
- `CS=13`
- `DC=12`
- `RST=11`
- `BL=10`

There is also [Examples/EngineDemo/CMakeLists.txt](/home/smikhai/repo/rp_pico_display_engine/Examples/EngineDemo/CMakeLists.txt:1), which defaults to:

- Board: `Pico 2` / `RP2350` (`PICO_BOARD=pico2`)
- Controller: `DISPLAY_TYPE_ST7789`
- SPI port: `spi1`
- `MOSI=15`
- `SCK=14`
- `CS=13`
- `DC=12`
- `RST=11`
- `BL=10`

So the produced `.uf2` must be flashed to the board selected by that example's CMake config, and the display must be wired to the pins listed there.

## API Contract (Important)

- Every successful `display_begin_paint_*()` must be followed by `display_end_paint()`
- Do not call a second `begin` until the current paint section is closed
- Do not call `display_submit()` manually inside an open paint section

Detailed usage scenarios: `SCENARIOS.ru.md`.
