#include "display/render/sine_wave.h"
#include <math.h>

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
