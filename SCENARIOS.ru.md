# Сценарии использования display engine

В движке используется явный контракт рисования:
- `display_begin_paint_try()` или `display_begin_paint_blocking()` открывает кадр и даёт буфер.
- `display_end_paint()` закрывает кадр и передаёт его в вывод (в SAFE+2 может отложить на один кадр, если DMA занят).

Главное правило: каждый успешный `begin` должен завершаться `display_end_paint()`.

## 1. Обновление 50 Гц без разрывов (рекомендуется)

Что получить:
- Тик 50 Гц от таймера.
- Если экран занят, кадр пропускается.
- Без разрывов и без блокировки основного цикла.

Конфигурация:
- `mode = DISPLAY_MODE_SAFE`
- `buffer_count = 1` или `2`

Последовательность:
1. `display_init(&cfg)`
2. В `main` на каждом тике:
   - `buf = display_begin_paint_try()`
   - если `buf == NULL` -> пропуск тика
   - рисование
   - `display_end_paint()`

## 2. Простой блокирующий цикл

Что получить:
- Максимально короткий код без проверок `NULL`.

Конфигурация:
- `mode = DISPLAY_MODE_SAFE`
- обычно `buffer_count = 1`

Последовательность:
1. `display_init(&cfg)`
2. В цикле:
   - `buf = display_begin_paint_blocking()`
   - рисование
   - `display_end_paint()`

Минус:
- Поток может ждать буфер и временно не выполнять другую работу.

## 3. Неблокирующая анимация/UI

Что получить:
- Главный цикл всегда остаётся отзывчивым.
- Пока DMA выводит буфер A, можно рисовать буфер B без блокировки.

Конфигурация:
- `mode = DISPLAY_MODE_SAFE`
- лучше `buffer_count = 2`

Последовательность:
1. `display_init(&cfg)`
2. В цикле:
   - `buf = display_begin_paint_try()`
   - если `buf == NULL` -> делаем другую логику и идём дальше
   - рисование
   - `display_end_paint()`

## 4. RAW-режим (низкоуровневый ручной контроль)

Что получить:
- Полный ручной контроль swap в RAW-режиме.

Конфигурация:
- `mode = DISPLAY_MODE_RAW`
- `buffer_count = 2`

Последовательность:
1. `buf = display_begin_paint_try()`
2. если `buf == NULL` -> пропуск итерации
3. рисование
4. `display_swap_buffers()`
5. `display_end_paint()`

Примечание:
- В RAW swap остаётся ручным, а отправка кадра выполняется через `display_end_paint()`.

## 5. Что нельзя делать

1. Делать второй `begin`, не закрыв первый `display_end_paint()`.
2. Забывать `display_end_paint()` после успешного `begin`.
3. Вызывать `display_submit()` вручную внутри открытой paint-секции.

## 6. Шаблон 50 Гц

```c
static volatile bool frame_tick_due = false;

static bool frame_timer_cb(repeating_timer_t* t) {
    (void)t;
    frame_tick_due = true;
    return true;
}

while (1) {
    if (!frame_tick_due) {
        __asm volatile ("wfi");
        continue;
    }
    frame_tick_due = false;

    uint16_t* buf = display_begin_paint_try();
    if (!buf) {
        continue;
    }

    // draw(buf)

    bool ok = display_end_paint();
    hard_assert(ok);
}
```
