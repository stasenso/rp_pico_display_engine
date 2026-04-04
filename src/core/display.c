#include "display.h"
#include "display_transport.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"


#ifndef DISPLAY_MAX_BUFFERS
#define DISPLAY_MAX_BUFFERS 2
#endif


typedef struct
{
    uint16_t width;
    uint16_t height;

    uint8_t buffer_count;
    display_mode_t mode;

    volatile bool dma_busy;

    uint16_t* buffers[DISPLAY_MAX_BUFFERS];

    uint8_t draw_index;
    uint8_t scanout_index;
    uint8_t pending_scanout_index;
    bool pending_submit;
    bool paint_in_progress;

} display_context_t;


static display_context_t ctx;

/*
 * Назначение:
 *   Пробрасывает аппаратное прерывание DMA в основной обработчик модуля дисплея.
 */
static void display_dma_irq_trampoline(void)
{
    display_dma_irq_handler();
}


/*
 * Назначение:
 *   Пытается запустить отложенный submit, если DMA уже освободился.
 */
static void display_kick_pending_if_possible(void)
{
    if (!ctx.pending_submit || ctx.dma_busy)
    {
        return;
    }

    /* Продвигаем отложенный кадр в scanout и возвращаем draw на второй буфер. */
    ctx.scanout_index = ctx.pending_scanout_index;
    if (ctx.buffer_count == 2)
    {
        ctx.draw_index = (uint8_t)(ctx.scanout_index ^ 1u);
    }
    ctx.pending_submit = false;

    size_t pixels = (size_t)ctx.width * ctx.height;
    ctx.dma_busy = true;
    display_transport_start_frame(ctx.width, ctx.height, ctx.buffers[ctx.scanout_index], pixels);
}


/* ============================================================
   === Обработчик DMA IRQ (позже подключить к реальному IRQ)
   ============================================================ */

/*
 * Назначение:
 *   Обрабатывает сигнал «кадр отправлен»: очищает флаг прерывания и помечает,
 *   что DMA больше не занят.
 */
void display_dma_irq_handler(void)
{
    display_transport_complete_irq();

    /* Отмечаем, что DMA завершил передачу и шина свободна. */
    ctx.dma_busy = false;
}


/* ============================================================
   === Публичный API
   ============================================================ */

/*
 * Назначение:
 *   Запускает модуль дисплея: сохраняет настройки, создаёт буферы кадра и
 *   подготавливает железо.
 *
 * Параметры:
 *   cfg (const display_config_t*) - указатель на конфигурацию; не NULL.
 *   Ожидаемые значения полей:
 *     cfg->buffer_count (uint8_t): 1..DISPLAY_MAX_BUFFERS.
 *     cfg->width (uint16_t): 0..65535.
 *     cfg->height (uint16_t): 0..65535.
 *     cfg->mode (display_mode_t): DISPLAY_MODE_SAFE или DISPLAY_MODE_RAW.
 */
void display_init(const display_config_t* cfg)
{
    /* Проверяем, что передан валидный указатель на конфигурацию. */
    assert(cfg != NULL);
    /* Минимум один буфер обязателен. */
    assert(cfg->buffer_count >= 1);
    /* Количество буферов не должно превышать лимит сборки. */
    assert(cfg->buffer_count <= DISPLAY_MAX_BUFFERS);

    /* Полностью очищаем глобальный контекст перед инициализацией. */
    memset(&ctx, 0, sizeof(ctx));

    /* Копируем размеры кадра из конфигурации. */
    ctx.width  = cfg->width;
    ctx.height = cfg->height;

    /* Копируем режим и параметры буферизации. */
    ctx.buffer_count = cfg->buffer_count;
    ctx.mode         = cfg->mode;

    /* Считаем количество пикселей в одном кадре. */
    size_t pixels = (size_t)ctx.width * ctx.height;
    /* Считаем объём одного буфера в байтах (RGB565 = 2 байта/пиксель). */
    size_t bytes  = pixels * sizeof(uint16_t);

    /* Выделяем и обнуляем каждый кадровый буфер. */
    for (uint8_t i = 0; i < ctx.buffer_count; i++)
    {
        ctx.buffers[i] = malloc(bytes);
        assert(ctx.buffers[i] != NULL);
        memset(ctx.buffers[i], 0, bytes);
    }

    /* SAFE+double-buffer: draw и scanout должны смотреть на разные буферы. */
    ctx.scanout_index = 0;
    ctx.draw_index = (ctx.mode == DISPLAY_MODE_SAFE && ctx.buffer_count == 2) ? 1u : 0u;
    ctx.pending_scanout_index = 0;
    ctx.pending_submit = false;

    /* DMA в начале свободен. */
    ctx.dma_busy = false;
    ctx.paint_in_progress = false;

    /* Инициализируем транспортный слой дисплея и backend контроллера. */
    display_transport_init(ctx.width, ctx.height, display_dma_irq_trampoline);
}


/* ------------------------------------------------------------ */

/*
 * Назначение:
 *   Пытается начать новую фазу рисования кадра и выдать буфер для отрисовки.
 *
 * Возвращаемое значение:
 *   uint16_t* - буфер для рисования.
 *   NULL - рисование уже начато ранее или буфер пока недоступен.
 */
uint16_t* display_begin_paint_try(void)
{
    display_kick_pending_if_possible();

    if (ctx.paint_in_progress)
    {
        return NULL;
    }

    /* В SAFE+single-buffer нельзя рисовать, пока DMA читает тот же буфер. */
    if (ctx.mode == DISPLAY_MODE_SAFE &&
        ctx.buffer_count == 1 &&
        ctx.dma_busy)
    {
        return NULL;
    }

    /* SAFE+double-buffer: при отложенном кадре свободного draw-буфера нет. */
    if (ctx.mode == DISPLAY_MODE_SAFE &&
        ctx.buffer_count == 2 &&
        ctx.pending_submit)
    {
        return NULL;
    }

    ctx.paint_in_progress = true;
    return ctx.buffers[ctx.draw_index];
}


/* ------------------------------------------------------------ */

/*
 * Назначение:
 *   Ждёт доступный буфер и начинает фазу рисования кадра.
 *
 * Параметры:
 *   Нет.
 *
 * Возвращаемое значение:
 *   uint16_t* - валидный буфер для рисования.
 */
uint16_t* display_begin_paint_blocking(void)
{
    /* Если paint уже начат, blocking-вызов здесь приведёт к вечному ожиданию. */
    hard_assert(!ctx.paint_in_progress);

    uint16_t* buf = NULL;

    while (buf == NULL)
    {
        buf = display_begin_paint_try();
        tight_loop_contents();
    }

    return buf;
}


/* ------------------------------------------------------------ */

/*
 * Назначение:
 *   Завершает фазу рисования кадра и отправляет кадр на экран.
 *
 * Возвращаемое значение:
 *   bool - true, если кадр принят к выводу (запущен сразу или поставлен в очередь).
 *   bool - false, если нарушен порядок begin/end или очередь уже заполнена.
 */
bool display_end_paint(void)
{
    if (!ctx.paint_in_progress)
    {
        return false;
    }

    ctx.paint_in_progress = false;
    return display_submit();
}


/*
 * Назначение:
 *   Возвращает буфер, который сейчас выбран для показа на экране.
 *
 * Возвращаемое значение:
 *   uint16_t* - указатель на текущий scanout-буфер.
 */
uint16_t* display_get_scanout_buffer(void)
{
    /* Возвращаем буфер, который назначен на вывод в DMA. */
    return ctx.buffers[ctx.scanout_index];
}


/* ------------------------------------------------------------ */

/*
 * Назначение:
 *   Меняет местами «буфер рисования» и «буфер показа» вручную.
 *
 * Возвращаемое значение:
 *   bool - true при успешном swap.
 *   bool - false, если не RAW, меньше двух буферов или DMA занят.
 */
bool display_swap_buffers(void)
{
    /* Операция доступна только в RAW-режиме. */
    if (ctx.mode != DISPLAY_MODE_RAW)
        return false;

    /* Для swap нужно минимум два буфера. */
    if (ctx.buffer_count < 2)
        return false;

    /* Во время активной DMA-передачи менять роли буферов нельзя. */
    if (ctx.dma_busy)
        return false;

    /* Меняем местами индексы draw и scanout. */
    uint8_t tmp = ctx.draw_index;
    ctx.draw_index = ctx.scanout_index;
    ctx.scanout_index = tmp;

    /* Обмен выполнен успешно. */
    return true;
}


/* ------------------------------------------------------------ */

/*
 * Назначение:
 *   Запускает отправку текущего готового кадра на экран.
 *
 * Возвращаемое значение:
 *   bool - true, если кадр принят (старт DMA или постановка в очередь SAFE+double-buffer).
 *   bool - false, если сейчас открыта paint-секция, DMA занят в других режимах
 *          или очередь SAFE+double-buffer уже заполнена.
 */
bool display_submit(void)
{
    display_kick_pending_if_possible();

    /* Пока идёт paint-секция, submit допускается только через end_paint. */
    if (ctx.paint_in_progress)
        return false;

    /* SAFE+double-buffer: при занятом DMA кадр ставится в очередь. */
    if (ctx.mode == DISPLAY_MODE_SAFE &&
        ctx.buffer_count == 2)
    {
        if (ctx.dma_busy)
        {
            if (ctx.pending_submit)
            {
                return false;
            }

            ctx.pending_scanout_index = ctx.draw_index;
            ctx.pending_submit = true;
            return true;
        }

        ctx.scanout_index = ctx.draw_index;
        ctx.draw_index = (uint8_t)(ctx.scanout_index ^ 1u);
    }
    else if (ctx.dma_busy)
    {
        /* Для остальных режимов submit во время DMA запрещён. */
        return false;
    }

    /* Считаем количество пикселей для DMA-передачи. */
    size_t pixels = (size_t)ctx.width * ctx.height;

    /* Помечаем DMA как занятый до прихода IRQ завершения. */
    ctx.dma_busy = true;

    /* Запускаем передачу scanout-буфера в дисплей. */
    display_transport_start_frame(ctx.width, ctx.height, ctx.buffers[ctx.scanout_index], pixels);

    /* Запуск принят. */
    return true;
}


/* ------------------------------------------------------------ */

/*
 * Назначение:
 *   Сообщает, свободен ли дисплей для отправки следующего кадра.
 *
 * Возвращаемое значение:
 *   bool - true, если DMA не занят.
 *   bool - false, если DMA выполняет передачу.
 */
bool display_ready(void)
{
    display_kick_pending_if_possible();

    /* Готовность есть, когда нет активной DMA и нет отложенного кадра. */
    return !ctx.dma_busy && !ctx.pending_submit;
}


/* ------------------------------------------------------------ */

/*
 * Назначение:
 *   Останавливает выполнение, пока текущий кадр полностью не уйдёт на экран.
 */
void display_wait(void)
{
    /* Ждём полного опустошения очереди кадров и завершения DMA. */
    while (true)
    {
        display_kick_pending_if_possible();
        if (!ctx.dma_busy && !ctx.pending_submit)
        {
            break;
        }

        tight_loop_contents();
    }
}
