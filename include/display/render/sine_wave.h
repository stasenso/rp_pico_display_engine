#pragma once

#include "display/render/context.h"

#ifdef __cplusplus
extern "C" {
#endif

void render_sine_wave(
    render_ctx_t* ctx,
    uint16_t num_points,
    int amplitude,
    float frequency,
    int offset_x,
    int offset_y,
    float phase_shift,
    uint16_t color
);

#ifdef __cplusplus
}
#endif
