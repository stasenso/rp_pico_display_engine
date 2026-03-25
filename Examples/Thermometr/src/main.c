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

        render_clear(&rc, RGB16(9,19,9));
        render_grid(&rc, 20, 20, 40, RGB16(12,26,13));
        render_sine_wave(&rc, WIDTH, 50, 2.0f, 0, HEIGHT / 2, phase, RGB16(0,255,0));
        draw_string(&rc, 50, 90, L"Проверка кириллицы", RGB565(255, 255, 255));
        draw_string(&rc, 45, 110, L"Proverka latinyanskogo", RGB565(0, 0, 255));
        draw_string(&rc, 35, 130, L"1234567890!@#$%%^&*()", RGB565(255, 0, 0));
        phase += 0.08f;
        if (phase > 6.2831853f)
        {
            phase -= 6.2831853f;
        }
    }
}
