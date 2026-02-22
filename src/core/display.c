#include "display.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>


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


/* ============================================================
   === Hardware abstraction (to be implemented later)
   ============================================================ */

static void hw_init(uint16_t width, uint16_t height)
{
    /* TODO:
       - init SPI
       - init DMA
       - init Core1 if needed
       - register DMA IRQ
    */
}

static void hw_start_dma(uint16_t* buffer, size_t pixel_count)
{
    /* TODO:
       - assert dma not active at HW level
       - assert CS low
       - configure DMA transfer
       - start transfer
    */

    (void)buffer;
    (void)pixel_count;
}

static void hw_raise_cs(void)
{
    /* TODO: raise chip select */
}


/* ============================================================
   === DMA IRQ handler (must be wired to real IRQ later)
   ============================================================ */

void display_dma_irq_handler(void)
{
    /* TODO: clear DMA interrupt flag */

    hw_raise_cs();

    ctx.dma_busy = false;
    ctx.frame_done_pending = true;
}


/* ============================================================
   === Public API
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

uint16_t* display_get_draw_buffer(void)
{
    if (ctx.mode == DISPLAY_MODE_SAFE &&
        ctx.buffer_count == 1)
    {
        while (ctx.dma_busy)
        {
            /* busy wait */
        }
    }

    return ctx.buffers[ctx.draw_index];
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
        /* busy wait */
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