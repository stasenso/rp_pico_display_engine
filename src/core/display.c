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
    volatile bool frame_done_pending;

    display_frame_done_cb_t frame_done_cb;

    uint16_t* buffers[DISPLAY_MAX_BUFFERS];

    uint8_t draw_index;
    uint8_t scanout_index;

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

static inline void st7789_send_command(uint8_t cmd)
{
    /* DC=0: передаём байт команды контроллеру дисплея */
    gpio_put(PIN_DC, 0);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(PIN_CS, 1);
}

static inline void st7789_send_data_bytes(const uint8_t* data, size_t len)
{
    /* DC=1: передаём полезные данные команды */
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, data, len);
    gpio_put(PIN_CS, 1);
}

static inline void st7789_send_data_u8(uint8_t data)
{
    st7789_send_data_bytes(&data, 1);
}

static void display_dma_irq_trampoline(void)
{
    display_dma_irq_handler();
}


/* ============================================================
   === Абстракция оборудования (реализовать позже)
   ============================================================ */

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

static void hw_raise_cs(void)
{
    gpio_put(PIN_CS, 1);
}


/* ============================================================
   === Обработчик DMA IRQ (позже подключить к реальному IRQ)
   ============================================================ */

void display_dma_irq_handler(void)
{
    if (dma_chan >= 0)
    {
        /* Сбрасываем флаг IRQ у текущего DMA-канала */
        dma_hw->ints0 = 1u << (uint)dma_chan;
    }

    hw_raise_cs();

    ctx.dma_busy = false;
    ctx.frame_done_pending = true;
}


/* ============================================================
   === Публичный API
   ============================================================ */

void display_init(const display_config_t* cfg)
{
    assert(cfg != NULL);
    assert(cfg->buffer_count >= 1);
    assert(cfg->buffer_count <= DISPLAY_MAX_BUFFERS);

    memset(&ctx, 0, sizeof(ctx));

    ctx.width  = cfg->width;
    ctx.height = cfg->height;

    ctx.buffer_count = cfg->buffer_count;
    ctx.mode         = cfg->mode;
    ctx.frame_done_cb = cfg->frame_done_cb;

    size_t pixels = (size_t)ctx.width * ctx.height;
    size_t bytes  = pixels * sizeof(uint16_t);

    for (uint8_t i = 0; i < ctx.buffer_count; i++)
    {
        ctx.buffers[i] = malloc(bytes);
        assert(ctx.buffers[i] != NULL);
        memset(ctx.buffers[i], 0, bytes);
    }

    ctx.draw_index    = 0;
    ctx.scanout_index = 0;

    ctx.dma_busy = false;
    ctx.frame_done_pending = false;

    hw_init(ctx.width, ctx.height);
}


/* ------------------------------------------------------------ */

uint16_t* display_try_acquire_draw_buffer(void)
{
    if (ctx.mode == DISPLAY_MODE_SAFE &&
        ctx.buffer_count == 1)
    {
        if (ctx.dma_busy)
        {
            return NULL;
        }
    }

    return ctx.buffers[ctx.draw_index];
}


/* ------------------------------------------------------------ */

uint16_t* display_acquire_draw_buffer_blocking(void)
{
    uint16_t* buf = NULL;

    while (buf == NULL)
    {
        buf = display_try_acquire_draw_buffer();
    }

    return buf;
}


/* ------------------------------------------------------------ */

uint16_t* display_get_draw_buffer(void)
{
    return display_try_acquire_draw_buffer();
}


/* ------------------------------------------------------------ */

uint16_t* display_get_scanout_buffer(void)
{
    return ctx.buffers[ctx.scanout_index];
}


/* ------------------------------------------------------------ */

bool display_swap_buffers(void)
{
    if (ctx.mode != DISPLAY_MODE_RAW)
        return false;

    if (ctx.buffer_count < 2)
        return false;

    if (ctx.dma_busy)
        return false;

    uint8_t tmp = ctx.draw_index;
    ctx.draw_index = ctx.scanout_index;
    ctx.scanout_index = tmp;

    return true;
}


/* ------------------------------------------------------------ */

bool display_submit(void)
{
    if (ctx.dma_busy)
        return false;

    if (ctx.mode == DISPLAY_MODE_SAFE &&
        ctx.buffer_count == 2)
    {
        uint8_t tmp = ctx.draw_index;
        ctx.draw_index = ctx.scanout_index;
        ctx.scanout_index = tmp;
    }

    size_t pixels = (size_t)ctx.width * ctx.height;

    ctx.dma_busy = true;

    hw_start_dma(
        ctx.buffers[ctx.scanout_index],
        pixels
    );

    return true;
}


/* ------------------------------------------------------------ */

bool display_ready(void)
{
    return !ctx.dma_busy;
}


/* ------------------------------------------------------------ */

void display_wait(void)
{
    while (ctx.dma_busy)
    {
        /* активное ожидание */
    }
}


/* ------------------------------------------------------------ */

void display_poll(void)
{
    if (ctx.frame_done_pending)
    {
        ctx.frame_done_pending = false;

        if (ctx.frame_done_cb)
        {
            ctx.frame_done_cb();
        }
    }
}
