# RP Pico Display Engine

A lightweight C display engine for `RP2040/RP2350` and `ST7789` displays (SPI + DMA) with an explicit `begin/end paint` frame contract.

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

## Quick Start

### 1. Safe mode with a single display loop

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

### 2. Safe mode with two buffers and deferred submit support

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

### 3. Primitive and font headers + output example

Minimal include set:

```c
#include "display/display.h"
#include "display/renderer.h"          // aggregates context + line + grid + sine_wave + bezier
#include "Font/font_data.h"            // draw_string(), draw_char(), get_char_width()
```

Equivalent per-header includes:

```c
#include "display/render/context.h"
#include "display/render/line.h"
#include "display/render/grid.h"
#include "display/render/sine_wave.h"
#include "display/render/bezier.h"
#include "Font/font_data.h"
```

Primitive + text render example:

```c
render_ctx_t rc;
render_begin(&rc, buf, 320, 240);

render_clear(&rc, RGB16(9, 19, 9));
render_grid(&rc, 20, 20, 40, RGB16(12, 26, 13));
render_line(&rc, 0, 0, 319, 239, RGB16(255, 0, 0));
render_sine_wave(&rc, 960, 100, 2.0f, 0, 120, phase, RGB16(0, 255, 0));

int px[4] = {20, 80, 140, 220};
int py[4] = {200, 120, 220, 160};
render_bezier(&rc, px, py, 4, RGB16(255, 255, 0));

draw_string(&rc, 16, 16, L"Cyrillic check", RGB565(255, 255, 255));
```

## Build Example (Pico SDK)

```bash
cd Examples/Thermometr
mkdir -p build && cd build
cmake ..
cmake --build .
```

Display pins are configured in `Examples/Thermometr/CMakeLists.txt` via `target_compile_definitions(...)`:
`SPI_PORT`, `PIN_MOSI`, `PIN_SCK`, `PIN_CS`, `PIN_DC`, `PIN_RST`, `PIN_BL`.

## API Contract (Important)

- Every successful `display_begin_paint_*()` must be followed by `display_end_paint()`
- Do not call a second `begin` until the current paint section is closed
- Do not call `display_submit()` manually inside an open paint section

Detailed usage scenarios: `SCENARIOS.ru.md`.
