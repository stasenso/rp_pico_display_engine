#include "pico/stdlib.h"
#include "display/display.h"
#include "display/render/context.h"
#include "display/render/grid.h"
#include "display/render/sine_wave.h"
#include "Font/font_data.h"

#define WIDTH   320
#define HEIGHT  240

static void on_frame_done(void)
{
    display_submit();
}

int main(void)
{
    stdio_init_all();

    display_config_t cfg = {
        .width = WIDTH,
        .height = HEIGHT,
        .buffer_count = 1,
        .mode = DISPLAY_MODE_SAFE,
        .frame_done_cb = on_frame_done
    };

    display_init(&cfg);
    display_submit();

    float phase = 0.0f;
    render_ctx_t rc;

    while (1)
    {
        display_poll();

        uint16_t* buf = display_get_draw_buffer();
        render_begin(&rc, buf, WIDTH, HEIGHT);

        render_clear(&rc, RGB16(9, 19, 9));
        render_grid(&rc, 20, 20, 40, RGB16(12, 26, 13));
        render_sine_wave(&rc, WIDTH, 50, 2.0f, 0, HEIGHT / 2, phase, RGB16(0, 255, 0));
        draw_string(&rc, 12, 12, L"rp_pico_display_engine", RGB565(255, 255, 255));

        phase += 0.08f;
        if (phase > 6.2831853f)
        {
            phase -= 6.2831853f;
        }
    }
}
