#include "display/renderer.h"
#include <math.h>
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

void render_line(render_ctx_t* ctx, int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 >= y0) ? (y0 - y1) : (y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true)
    {
        render_pixel(ctx, x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;

        int e2 = err * 2;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void render_grid(render_ctx_t* ctx, uint16_t x, uint16_t y, uint16_t step, uint16_t color)
{
    if (step == 0 || ctx->width == 0 || ctx->height == 0)
        return;

    for (uint16_t v = x; v < ctx->width; v = (uint16_t)(v + step))
    {
        render_line(ctx, v, 0, v, (int)ctx->height - 1, color);
    }

    for (uint16_t h = y; h < ctx->height; h = (uint16_t)(h + step))
    {
        render_line(ctx, 0, h, (int)ctx->width - 1, h, color);
    }
}

void render_sine_wave(
    render_ctx_t* ctx,
    uint16_t num_points,
    int amplitude,
    float frequency,
    int offset_x,
    int offset_y,
    float phase_shift,
    uint16_t color
)
{
    if (num_points < 2 || ctx->width == 0 || ctx->height == 0)
        return;

    float step = (2.0f * (float)M_PI * frequency) / (float)(num_points - 1u);
    float x_step = (float)ctx->width / (float)(num_points - 1u);

    for (uint16_t i = 0; i < num_points; ++i)
    {
        int x = offset_x + (int)((float)i * x_step);
        int y = offset_y + (int)((float)amplitude * sinf((float)i * step + phase_shift));
        render_pixel(ctx, x, y, color);
    }
}

static float bernstein(int i, int n, float t)
{
    float binomial = 1.0f;
    for (int j = 0; j < i; ++j)
    {
        binomial *= (float)(n - j) / (float)(j + 1);
    }
    return binomial * powf(t, (float)i) * powf(1.0f - t, (float)(n - i));
}

void render_bezier(
    render_ctx_t* ctx,
    const int* points_x,
    const int* points_y,
    size_t num_points,
    uint16_t color
)
{
    if (num_points < 2)
        return;

    const int steps = 1000;
    for (int s = 0; s <= steps; ++s)
    {
        float t = (float)s / (float)steps;
        float x = 0.0f;
        float y = 0.0f;

        for (size_t i = 0; i < num_points; ++i)
        {
            float b = bernstein((int)i, (int)num_points - 1, t);
            x += b * (float)points_x[i];
            y += b * (float)points_y[i];
        }

        render_pixel(ctx, (int)(x + 0.5f), (int)(y + 0.5f), color);
    }
}

