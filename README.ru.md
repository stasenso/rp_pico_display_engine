# RP Pico Display Engine

Лёгкий C-движок вывода для `RP2040/RP2350` и дисплеев `ST7789` / `ILI9341` (SPI + DMA) с явным контрактом кадра `begin/end paint`.

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

## Что должно быть установлено заранее

- `git`
- `cmake` версии `3.18.4+`
- средство сборки, которое поддерживает CMake (`make` по умолчанию на Unix-подобных системах)
- `Pico SDK` и переменная окружения `PICO_SDK_PATH`, указывающая на него
- ARM toolchain `arm-none-eabi-gcc`

## Поддерживаемые дисплеи и wiring по умолчанию

Поддерживаются два backend-контроллера:

- `DISPLAY_TYPE_ST7789`
- `DISPLAY_TYPE_ILI9341`

Если не передавать compile definitions для дисплея, библиотека использует значения по умолчанию из [`src/core/display_driver.h`](src/core/display_driver.h):

- Контроллер: `DISPLAY_TYPE_ST7789`
- SPI порт: `spi0`
- `MOSI=19`
- `SCK=18`
- `CS=17`
- `DC=22`
- `RST=13`
- `BL=12`

То есть без переопределения пинов и `DISPLAY_TYPE` прошивка рассчитана на `ST7789`, подключённый к этим GPIO у `RP2040/RP2350`.

## Быстрый старт

### 1. Подключение исходников через git submodule (рекомендуется)

Практичный вариант интеграции: добавить этот репозиторий в проект как вендорную зависимость через `git submodule` и собрать `display_engine` из вашего основного `CMakeLists.txt`.
Подключать все примитивы сразу не обязательно: оставьте только нужные и раскомментируйте остальные позже.

Создайте новый проект и добавьте библиотеку как submodule:

```bash
mkdir pico_display_app
cd pico_display_app
git init
git submodule add https://github.com/stasenso/rp_pico_display_engine.git external/rp_pico_display_engine
git submodule update --init --recursive
mkdir -p src
```

Убедитесь, что переменная `PICO_SDK_PATH` выставлена:

```bash
export PICO_SDK_PATH=/absolute/path/to/pico-sdk
```

Создайте `CMakeLists.txt` в корне проекта:

```bash
cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.18.4)

# Замените на `pico` для плат на RP2040 или оставьте `pico2` для Pico 2 / RP2350.
set(PICO_BOARD pico2 CACHE STRING "Pico SDK target board")

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
    # Раскомментируйте при необходимости:
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

Создайте `src/main.c`:

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

Соберите проект:

```bash
cmake -S . -B build
cmake --build build
```

### 2. Как менять пины и тип дисплея

Задавайте compile definitions на цели `display_engine`:

- `DISPLAY_SPI_PORT` (`spi0` или `spi1`)
- `DISPLAY_PIN_MOSI`, `DISPLAY_PIN_SCK`, `DISPLAY_PIN_CS`, `DISPLAY_PIN_DC`, `DISPLAY_PIN_RST`, `DISPLAY_PIN_BL`
- `DISPLAY_TYPE` (`DISPLAY_TYPE_ST7789` или `DISPLAY_TYPE_ILI9341`)

Если определения не заданы, используются такие значения по умолчанию из [`src/core/display_driver.h`](src/core/display_driver.h):

- `DISPLAY_TYPE=DISPLAY_TYPE_ST7789`
- `DISPLAY_SPI_PORT=spi0`
- `DISPLAY_PIN_MOSI=19`
- `DISPLAY_PIN_SCK=18`
- `DISPLAY_PIN_CS=17`
- `DISPLAY_PIN_DC=22`
- `DISPLAY_PIN_RST=13`
- `DISPLAY_PIN_BL=12`

### 3. Режим SAFE с одним буфером в цикле

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

### 4. Режим SAFE с двумя буферами и возможным отложенным выводом

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

### 5. Заголовки примитивов и шрифтов + пример вывода

Минимальный набор:

```c
#include "display/display.h"
#include "display/render/context.h"
#include "display/render/line.h"
#include "Font/font_data.h"            // draw_string(), draw_char(), get_char_width()
```

Опциональные заголовки (раскомментируйте при необходимости):

```c
// #include "display/render/grid.h"
// #include "display/render/sine_wave.h"
// #include "display/render/bezier.h"
```

Пример рендера примитивов и текста:

```c
render_ctx_t rc;
render_begin(&rc, buf, 320, 240);

render_clear(&rc, RGB16(9, 19, 9));
render_line(&rc, 0, 0, 319, 239, RGB16(255, 0, 0));
// render_grid(&rc, 20, 20, 40, RGB16(12, 26, 13)); // раскомментируйте, если нужна сетка
// render_sine_wave(&rc, 960, 100, 2.0f, 0, 120, phase, RGB16(0, 255, 0)); // раскомментируйте при необходимости

int px[4] = {20, 80, 140, 220};
int py[4] = {200, 120, 220, 160};
// render_bezier(&rc, px, py, 4, RGB16(255, 255, 0)); // раскомментируйте при необходимости

draw_string(&rc, 16, 16, L"Проверка кириллицы", RGB565(255, 255, 255));
```

## Сборка примера (Pico SDK)

Если хотите собрать готовый пример из этого репозитория без создания собственного проекта:

```bash
cd Examples/Thermometr
mkdir -p build && cd build
cmake ..
cmake --build .
```

В этом примере целевая плата задаётся прямо в [`Examples/Thermometr/CMakeLists.txt`](Examples/Thermometr/CMakeLists.txt). Чтобы переключить плату, измените строку:

```cmake
set(PICO_BOARD pico2 CACHE STRING "Pico SDK target board")
```

Используйте `pico` для Pico на RP2040 или `pico2` для Pico 2 на RP2350. Конфигурация дисплея в этом же файле:

- Контроллер: `DISPLAY_TYPE_ILI9341`
- SPI порт: `spi1`
- `MOSI=15`
- `SCK=14`
- `CS=13`
- `DC=12`
- `RST=11`
- `BL=10`

Также есть [`Examples/EngineDemo/CMakeLists.txt`](Examples/EngineDemo/CMakeLists.txt), где по умолчанию выбрано:

- Плата: `Pico 2` / `RP2350` (`PICO_BOARD=pico2`)
- Контроллер: `DISPLAY_TYPE_ST7789`
- SPI порт: `spi1`
- `MOSI=15`
- `SCK=14`
- `CS=13`
- `DC=12`
- `RST=11`
- `BL=10`

То есть получившийся `.uf2` нужно заливать именно в ту плату, которая выбрана в `CMakeLists.txt` конкретного примера, а дисплей подключать к перечисленным там пинам.

## Контракт API (важно)

- Каждый успешный `display_begin_paint_*()` должен завершаться `display_end_paint()`
- Нельзя открывать второй `begin`, пока не закрыт первый
- Не вызывайте `display_submit()` вручную внутри открытой paint-секции

Подробные сценарии описаны в [`SCENARIOS.ru.md`](SCENARIOS.ru.md).
