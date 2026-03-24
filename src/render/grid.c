#include "display/render/grid.h"

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
