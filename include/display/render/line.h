#pragma once

#include "display/render/context.h"

#ifdef __cplusplus
extern "C" {
#endif

void render_line(render_ctx_t* ctx, int x0, int y0, int x1, int y1, uint16_t color);

#ifdef __cplusplus
}
#endif
