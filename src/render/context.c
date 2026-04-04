#include "display/render/context.h"
#include <stdbool.h>
#include "hardware/dma.h"

static int render_clear_dma_chan = -1;
static uint16_t render_clear_dma_color = 0;

static inline bool in_clip(const render_ctx_t* ctx, int x, int y)
{
    return x >= (int)ctx->clip_x0 &&
           x <= (int)ctx->clip_x1 &&
           y >= (int)ctx->clip_y0 &&
           y <= (int)ctx->clip_y1;
}

void render_begin(render_ctx_t* ctx, uint16_t* buf, uint16_t width, uint16_t height)
{
    ctx->buf = buf;
    ctx->width = width;
    ctx->height = height;
    render_reset_clip(ctx);
}

void render_set_clip(render_ctx_t* ctx, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    if (ctx->width == 0 || ctx->height == 0 || width == 0 || height == 0)
    {
        ctx->clip_x0 = 0;
        ctx->clip_y0 = 0;
        ctx->clip_x1 = 0;
        ctx->clip_y1 = 0;
        return;
    }

    uint32_t x1 = (uint32_t)x + (uint32_t)width - 1u;
    uint32_t y1 = (uint32_t)y + (uint32_t)height - 1u;

    if (x >= ctx->width || y >= ctx->height)
    {
        ctx->clip_x0 = 0;
        ctx->clip_y0 = 0;
        ctx->clip_x1 = 0;
        ctx->clip_y1 = 0;
        return;
    }

    if (x1 >= ctx->width)
        x1 = (uint32_t)ctx->width - 1u;
    if (y1 >= ctx->height)
        y1 = (uint32_t)ctx->height - 1u;

    ctx->clip_x0 = x;
    ctx->clip_y0 = y;
    ctx->clip_x1 = (uint16_t)x1;
    ctx->clip_y1 = (uint16_t)y1;
}

void render_reset_clip(render_ctx_t* ctx)
{
    if (ctx->width == 0 || ctx->height == 0)
    {
        ctx->clip_x0 = 0;
        ctx->clip_y0 = 0;
        ctx->clip_x1 = 0;
        ctx->clip_y1 = 0;
        return;
    }

    ctx->clip_x0 = 0;
    ctx->clip_y0 = 0;
    ctx->clip_x1 = (uint16_t)(ctx->width - 1u);
    ctx->clip_y1 = (uint16_t)(ctx->height - 1u);
}

void render_clear(render_ctx_t* ctx, uint16_t color)
{
    size_t total = (size_t)ctx->width * (size_t)ctx->height;
    if (total == 0)
    {
        return;
    }

    if (render_clear_dma_chan < 0)
    {
        render_clear_dma_chan = dma_claim_unused_channel(false);
    }

    if (render_clear_dma_chan >= 0)
    {
        dma_channel_config c = dma_channel_get_default_config((uint)render_clear_dma_chan);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
        channel_config_set_read_increment(&c, false);
        channel_config_set_write_increment(&c, true);
        channel_config_set_dreq(&c, DREQ_FORCE);

        render_clear_dma_color = color;

        dma_channel_configure(
            (uint)render_clear_dma_chan,
            &c,
            ctx->buf,
            &render_clear_dma_color,
            total,
            true
        );
        dma_channel_wait_for_finish_blocking((uint)render_clear_dma_chan);
        return;
    }

    for (size_t i = 0; i < total; ++i)
    {
        ctx->buf[i] = color;
    }
}

void render_pixel(render_ctx_t* ctx, int x, int y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= (int)ctx->width || y >= (int)ctx->height)
        return;

    if (!in_clip(ctx, x, y))
        return;

    ctx->buf[(size_t)y * ctx->width + (size_t)x] = color;
}
