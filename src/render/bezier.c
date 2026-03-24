#include "display/render/bezier.h"
#include <math.h>

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
