#include "pico/stdlib.h"
#include "display/display.h"
#include "display/renderer.h"


#define WIDTH   320
#define HEIGHT  240


static void on_frame_done(void)
{
    // В режиме SAFE смена буферов выполняется внутри submit()
    display_submit();
}


int main()
{
    stdio_init_all();

    display_config_t cfg = {
        .width  = WIDTH,
        .height = HEIGHT,
        .buffer_count = 1,
        .mode = DISPLAY_MODE_SAFE,
        .frame_done_cb = on_frame_done
    };

    display_init(&cfg);

    display_submit(); /* Запускаем конвейер кадра */

    float phase = 0.0f;
    render_ctx_t rc;

    while (1)
    {
        display_poll();

        /* SAFE + 1 буфер: ожидание освобождения внутри display_get_draw_buffer() */
        uint16_t* buf = display_get_draw_buffer();
        render_begin(&rc, buf, WIDTH, HEIGHT);

        render_clear(&rc, 0x10A2);
        render_grid(&rc, 20, 20, 40, 0x5ACB);
        render_sine_wave(&rc, WIDTH, 50, 2.0f, 0, HEIGHT / 2, phase, 0xF800);

        phase += 0.08f;
        if (phase > 6.2831853f)
        {
            phase -= 6.2831853f;
        }
    }
}
