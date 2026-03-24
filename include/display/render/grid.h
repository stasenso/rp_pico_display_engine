#pragma once

#include "display/render/line.h"

#ifdef __cplusplus
extern "C" {
#endif

void render_grid(render_ctx_t* ctx, uint16_t x, uint16_t y, uint16_t step, uint16_t color);

#ifdef __cplusplus
}
#endif
