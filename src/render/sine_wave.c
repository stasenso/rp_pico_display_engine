#include "display/render/sine_wave.h"
#include "display/render/line.h"
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

    int prev_x = offset_x;
    int prev_y = offset_y + (int)((float)amplitude * sinf(phase_shift));

    for (uint16_t i = 1; i < num_points; ++i)
    {
        int x = offset_x + (int)((float)i * x_step);
        int y = offset_y + (int)((float)amplitude * sinf((float)i * step + phase_shift));

        render_line(ctx, prev_x, prev_y, x, y, color);
        prev_x = x;
        prev_y = y;
    }
}
