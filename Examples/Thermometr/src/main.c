#include "pico/stdlib.h"
#include "display/display.h"


#define WIDTH   240
#define HEIGHT  240


static void on_frame_done(void)
{
    // В SAFE режиме swap выполняется внутри submit()
    display_submit();
}


static void render_test_pattern(uint16_t* buf)
{
    for (uint16_t y = 0; y < HEIGHT; y++)
    {
        for (uint16_t x = 0; x < WIDTH; x++)
        {
            uint16_t r = (x & 0x1F) << 11;
            uint16_t g = (y & 0x3F) << 5;
            uint16_t b = (x & 0x1F);

            buf[y * WIDTH + x] = r | g | b;
        }
    }
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

    // Рисуем первый кадр
    uint16_t* buf = display_get_draw_buffer();
    render_test_pattern(buf);

    display_submit();

    while (1)
    {
        display_poll();

        // Здесь можно обновлять содержимое буфера
        // SAFE + 1 buffer будет ждать окончания DMA
        buf = display_get_draw_buffer();
        render_test_pattern(buf);
    }
}