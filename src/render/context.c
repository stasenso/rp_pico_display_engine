#include "display/render/context.h"
#include <stdbool.h>

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
