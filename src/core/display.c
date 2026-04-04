#include "display.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/irq.h"


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
static int dma_chan = -1;

#ifndef SPI_PORT
#define SPI_PORT spi0
#endif

#ifndef PIN_MOSI
#define PIN_MOSI 19
#endif

#ifndef PIN_SCK
#define PIN_SCK 18
#endif

#ifndef PIN_CS
#define PIN_CS 17
#endif

#ifndef PIN_DC
#define PIN_DC 22
#endif

#ifndef PIN_RST
#define PIN_RST 13
#endif

#ifndef PIN_BL
#define PIN_BL 12
#endif

/*
 * Назначение:
 *   Отправляет дисплею служебную команду (например, сброс или смену режима).
 *
 * Параметры:
 *   cmd (uint8_t) - код команды ST7789, диапазон 0..255.
 */
static inline void st7789_send_command(uint8_t cmd)
{
    /* DC=0: передаём байт команды контроллеру дисплея */
    gpio_put(PIN_DC, 0);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(PIN_CS, 1);
}

/*
 * Назначение:
 *   Отправляет дисплею набор данных, относящихся к последней команде.
 *
 * Параметры:
 *   data (const uint8_t*) - указатель на буфер данных; должен быть валиден при len > 0.
 *   len (size_t) - количество передаваемых байтов, диапазон 0..SIZE_MAX.
 */
static inline void st7789_send_data_bytes(const uint8_t* data, size_t len)
{
    /* DC=1: передаём полезные данные команды */
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, data, len);
    gpio_put(PIN_CS, 1);
}

/*
 * Назначение:
 *   Отправляет дисплею один байт данных.
 *
 * Параметры:
 *   data (uint8_t) - байт данных, диапазон 0..255.
 */
static inline void st7789_send_data_u8(uint8_t data)
{
    st7789_send_data_bytes(&data, 1);
}

/*
 * Назначение:
 *   Пробрасывает аппаратное прерывание DMA в основной обработчик модуля дисплея.
 */
static void display_dma_irq_trampoline(void)
{
    display_dma_irq_handler();
}


/* ============================================================
   === Абстракция оборудования (реализовать позже)
   ============================================================ */

/*
 * Назначение:
 *   Полностью подготавливает железо для работы экрана: линии, SPI, контроллер и DMA.
 *
 * Параметры:
 *   width (uint16_t) - ширина кадра в пикселях, диапазон 0..65535.
 *   height (uint16_t) - высота кадра в пикселях, диапазон 0..65535.
 */
static void hw_init(uint16_t width, uint16_t height)
{
    /* Максимально быстрый SPI для вывода кадров на ST7789 */
    spi_init(SPI_PORT, 1000 * 100 * 625); /* 62.5 MHz */
    spi_set_format(
        SPI_PORT,
        8,
        SPI_CPOL_0,
        SPI_CPHA_0,
        SPI_MSB_FIRST
    );

    /* Линии данных SPI */
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);

    /* Управляющие пины дисплея */
    gpio_init(PIN_CS);
    gpio_init(PIN_DC);
    gpio_init(PIN_RST);
    gpio_init(PIN_BL);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_RST, GPIO_OUT);
    gpio_set_dir(PIN_BL, GPIO_OUT);

    gpio_put(PIN_CS, 1);
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_BL, 0);

    /* Аппаратный reset дисплея */
    gpio_put(PIN_RST, 0);
    sleep_ms(50);
    gpio_put(PIN_RST, 1);
    sleep_ms(50);

    st7789_send_command(0x01); /* SWRESET: программный сброс */
    sleep_ms(150);
    st7789_send_command(0x11); /* SLPOUT: выход из sleep mode */
    sleep_ms(150);

    st7789_send_command(0x36); /* MADCTL: ориентация/порядок осей и RGB/BGR */
    st7789_send_data_u8(0b10100000); /* Параметр из рабочей версии Thread.c */

    st7789_send_command(0x3A); /* COLMOD: формат пикселя */
    st7789_send_data_u8(0x55); /* 16 бит на пиксель (RGB565) */

    {
        uint16_t x_end = (width > 0) ? (uint16_t)(width - 1u) : 0u;
        uint16_t y_end = (height > 0) ? (uint16_t)(height - 1u) : 0u;
        uint8_t window[4];

        /* Окно вывода по X: [0 .. width-1] */
        st7789_send_command(0x2A); /* CASET: Column Address Set */
        window[0] = 0x00;
        window[1] = 0x00;
        window[2] = (uint8_t)(x_end >> 8);
        window[3] = (uint8_t)x_end;
        st7789_send_data_bytes(window, sizeof(window));

        /* Окно вывода по Y: [0 .. height-1] */
        st7789_send_command(0x2B); /* RASET: Row Address Set */
        window[2] = (uint8_t)(y_end >> 8);
        window[3] = (uint8_t)y_end;
        st7789_send_data_bytes(window, sizeof(window));
    }

    /* DMA -> SPI TX, чтобы выгружать кадр без участия CPU */
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config((uint)dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8); /* SPI в режиме 8 бит */
    channel_config_set_read_increment(&c, true);           /* читать массив буфера */
    channel_config_set_write_increment(&c, false);         /* писать в один регистр SPI DR */
    channel_config_set_dreq(&c, spi_get_dreq(SPI_PORT, true)); /* Тактирование от готовности SPI TX */

    dma_channel_configure(
        (uint)dma_chan,
        &c,
        &spi_get_hw(SPI_PORT)->dr, /* куда пишем: SPI data register */
        NULL,
        0,
        false
    );

    /* Прерывание по окончанию DMA-передачи кадра */
    dma_channel_set_irq0_enabled((uint)dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, display_dma_irq_trampoline);
    irq_set_enabled(DMA_IRQ_0, true);

    st7789_send_command(0x21); /* INVON: включить инверсию (как в старом коде) */
    st7789_send_command(0x29); /* DISPON: включить дисплей */
    gpio_put(PIN_BL, 1);
}

/*
 * Назначение:
 *   Готовит экран к приёму кадра и запускает быструю отправку пикселей через DMA.
 *
 * Параметры:
 *   buffer (uint16_t*) - указатель на буфер пикселей RGB565; должен быть валиден.
 *   pixel_count (size_t) - число пикселей для отправки, диапазон 0..SIZE_MAX.
 */
static void hw_start_dma(uint16_t* buffer, size_t pixel_count)
{
    uint16_t x_end = (ctx.width > 0) ? (uint16_t)(ctx.width - 1u) : 0u;
    uint16_t y_end = (ctx.height > 0) ? (uint16_t)(ctx.height - 1u) : 0u;
    uint8_t window[4];

    st7789_send_command(0x2A); /* CASET: Column Address Set */
    window[0] = 0x00;
    window[1] = 0x00;
    window[2] = (uint8_t)(x_end >> 8);
    window[3] = (uint8_t)x_end;
    st7789_send_data_bytes(window, sizeof(window));

    st7789_send_command(0x2B); /* RASET: Row Address Set */
    window[2] = (uint8_t)(y_end >> 8);
    window[3] = (uint8_t)y_end;
    st7789_send_data_bytes(window, sizeof(window));

    st7789_send_command(0x2C); /* RAMWR: Memory Write */

    /* DC=1 и CS=0 перед непрерывной DMA-передачей буфера */
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);

    dma_channel_set_read_addr((uint)dma_chan, buffer, false);
    dma_channel_set_trans_count((uint)dma_chan, pixel_count * sizeof(uint16_t), true);
}

/*
 * Назначение:
 *   Завершает текущую передачу в экран, поднимая линию выбора устройства (CS).
 */
static void hw_raise_cs(void)
{
    gpio_put(PIN_CS, 1);
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
    hw_start_dma(ctx.buffers[ctx.scanout_index], pixels);
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
    /* Проверяем, что DMA-канал действительно был выделен. */
    if (dma_chan >= 0)
    {
        /* Сбрасываем флаг IRQ у текущего DMA-канала */
        dma_hw->ints0 = 1u << (uint)dma_chan;
    }

    /* Поднимаем CS, завершая SPI-транзакцию кадра. */
    hw_raise_cs();

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

    /* Инициализируем аппаратный слой дисплея. */
    hw_init(ctx.width, ctx.height);
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
    hw_start_dma(
        ctx.buffers[ctx.scanout_index],
        pixels
    );

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
