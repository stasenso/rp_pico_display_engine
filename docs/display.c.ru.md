# display.c — актуальное описание (без display_poll)

## Коротко

Модуль управляет выводом кадров на ST7789 через DMA и работает в двух режимах:
- `DISPLAY_MODE_SAFE` — безопасная буферизация, с новым контрактом `begin/end paint`.
- `DISPLAY_MODE_RAW` — ручное управление буферами и отправкой.

Механизм `display_poll()` и callback завершения кадра удалён.

## Новый paint-контракт

### `display_begin_paint_try()`
- Неблокирующе пытается начать рисование нового кадра.
- Возвращает `NULL`, если буфер недоступен или paint уже начат.

### `display_begin_paint_blocking()`
- Блокирующе ждёт доступный буфер и начинает paint-секцию.

### `display_end_paint()`
- Завершает paint-секцию и отправляет кадр в вывод.
- Возвращает `false`, если нарушен порядок вызовов (`end` без `begin`) или DMA занят.

## SAFE и RAW

### SAFE
- Обычно используется `begin/end paint`.
- Движок сам держит корректный порядок отправки.

### RAW
- Для ручного контроля в RAW используется тот же begin/end-контракт,
  а swap выполняется явно:
  - `display_begin_paint_try()` / `display_begin_paint_blocking()`
  - `display_swap_buffers()`
  - `display_end_paint()`

## Что делает IRQ

`display_dma_irq_handler()`:
1. Сбрасывает IRQ-флаг DMA.
2. Завершает SPI-транзакцию (поднимает CS).
3. Снимает флаг `dma_busy`.

Никакой callback из основного цикла больше не нужен.

## Рекомендуемый цикл 50 Гц

```c
if (frame_tick_due) {
    frame_tick_due = false;

    uint16_t* buf = display_begin_paint_try();
    if (buf) {
        // draw
        bool ok = display_end_paint();
        hard_assert(ok);
    }
}
```

## Инварианты

1. После успешного `begin` обязательно вызвать `display_end_paint()`.
2. Нельзя начинать второй paint, пока первый не завершён.
3. Нельзя вызывать ручной `display_submit()` внутри открытой paint-секции.
