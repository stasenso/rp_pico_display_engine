# RP Pico Display Engine

Лёгкий C-движок вывода для `RP2040/RP2350` и дисплеев `ST7789` (SPI + DMA) с явным контрактом кадра `begin/end paint`.

## Возможности

- Режимы работы: `DISPLAY_MODE_SAFE` и `DISPLAY_MODE_RAW`
- 1 или 2 кадровых буфера (`buffer_count`)
- Неблокирующий и блокирующий захват кадра:
  - `display_begin_paint_try()`
  - `display_begin_paint_blocking()`
- Безопасное завершение кадра через `display_end_paint()`
- Набор примитивов рендера и шрифт с поддержкой кириллицы

## Структура репозитория

- `include/display/` - публичные заголовки движка и рендера
- `src/core/display.c` - ядро дисплейного движка
- `src/render/` - реализация примитивов
- `include/Font/`, `src/Font/` - данные шрифта и текстовый рендер
- `Examples/Thermometr/` - рабочий пример проекта на Pico SDK

## Быстрый старт

### 1. Safe mode с одним дисплеем в цикле

Подходит для простого цикла, где можно пропустить кадр, если буфер занят.

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

### 2. Safe mode с двумя дисплеями и возможным отложенным выводом

Режим `SAFE + buffer_count = 2`: пока DMA выводит один буфер, вы рисуете второй.  
Если DMA занят в момент `display_end_paint()`, кадр может быть отложен (очередь на 1 pending-кадр).

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
            // DMA/очередь заняты, делайте другую логику
            continue;
        }

        render_begin(&rc, buf, WIDTH, HEIGHT);
        render_clear(&rc, RGB16(8, 16, 8));
        // draw...

        bool accepted = display_end_paint();
        hard_assert(accepted); // false только при нарушении контракта
    }
}
```

### 3. Подключаемые `h`-файлы примитивов и шрифтов + пример вывода

Минимальный набор:

```c
#include "display/display.h"
#include "display/renderer.h"          // агрегирует context + line + grid + sine_wave + bezier
#include "Font/font_data.h"            // draw_string(), draw_char(), get_char_width()
```

Эквивалентно можно подключать по отдельности:

```c
#include "display/render/context.h"
#include "display/render/line.h"
#include "display/render/grid.h"
#include "display/render/sine_wave.h"
#include "display/render/bezier.h"
#include "Font/font_data.h"
```

Пример рендера примитивов и текста:

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

draw_string(&rc, 16, 16, L"Проверка кириллицы", RGB565(255, 255, 255));
```

## Сборка примера (Pico SDK)

```bash
cd Examples/Thermometr
mkdir -p build && cd build
cmake ..
cmake --build .
```

Пины дисплея задаются в `Examples/Thermometr/CMakeLists.txt` через `target_compile_definitions(...)`:
`SPI_PORT`, `PIN_MOSI`, `PIN_SCK`, `PIN_CS`, `PIN_DC`, `PIN_RST`, `PIN_BL`.

## Контракт API (важно)

- Каждый успешный `display_begin_paint_*()` должен завершаться `display_end_paint()`
- Нельзя открывать второй `begin`, пока не закрыт первый
- Не вызывайте `display_submit()` вручную внутри открытой paint-секции

Подробные сценарии: `SCENARIOS.ru.md`.
