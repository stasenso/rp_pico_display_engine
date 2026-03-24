#pragma once

#include "display/render/context.h"

#ifdef __cplusplus
extern "C" {
#endif

void render_bezier(
    render_ctx_t* ctx,
    const int* points_x,
    const int* points_y,
    size_t num_points,
    uint16_t color
);

#ifdef __cplusplus
}
#endif
