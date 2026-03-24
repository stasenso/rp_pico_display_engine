#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RGB565(R, G, B) \
    ((uint16_t)((((uint16_t)(R) & 0xF8u) << 8) | \
                (((uint16_t)(G) & 0xFCu) << 3) | \
                (((uint16_t)(B) & 0xF8u) >> 3)))

#define BSWAP16(X) \
    ((uint16_t)((((uint16_t)(X) & 0x00FFu) << 8) | \
                (((uint16_t)(X) & 0xFF00u) >> 8)))

/* RGB888 -> RGB565 with swapped byte order for ctx->buf */
#define RGB16(R, G, B) BSWAP16(RGB565((R), (G), (B)))

typedef struct
{
    uint16_t* buf;
    uint16_t width;
    uint16_t height;
    uint16_t clip_x0;
    uint16_t clip_y0;
    uint16_t clip_x1;
    uint16_t clip_y1;
} render_ctx_t;

void render_begin(render_ctx_t* ctx, uint16_t* buf, uint16_t width, uint16_t height);
void render_set_clip(render_ctx_t* ctx, uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void render_reset_clip(render_ctx_t* ctx);
void render_clear(render_ctx_t* ctx, uint16_t color);
void render_pixel(render_ctx_t* ctx, int x, int y, uint16_t color);

#ifdef __cplusplus
}
#endif
