#include "pico/stdlib.h"
#include "display/display.h"
#include "display/render/context.h"
#include "display/render/grid.h"
#include "display/render/sine_wave.h"
#include "Font/font_data.h"


#define WIDTH   320
#define HEIGHT  240
#define TEXT_H  16

static volatile bool frame_tick_due = false;
static repeating_timer_t frame_timer;

static bool frame_timer_cb(repeating_timer_t* t)
{
    (void)t;
    frame_tick_due = true; /* Схлопываем тики: если опоздали, лишние просто пропускаются */
    return true;
}

static inline void wait_for_irq(void)
{
    __asm volatile ("wfi");
}

static void on_frame_done(void) {}

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
    const int64_t frame_period_us = -16667; /* 60 Hz, отрицательное значение = стабильный период */
    bool timer_ok = add_repeating_timer_us(frame_period_us, frame_timer_cb, NULL, &frame_timer);
    hard_assert(timer_ok);

    float phase = 0.0f;
    int text1_y = 90;
    int text2_y = 110;
    int text3_y = 130;
    int text1_dy = 1;
    int text2_dy = 1;
    int text3_dy = 1;

    render_ctx_t rc;

    while (1)
    {
        display_poll();
        if (!frame_tick_due)
        {
            wait_for_irq();
            continue;
        }
        frame_tick_due = false;

        /* Пропускаем кадр, если предыдущий ещё уходит по DMA */
        if (!display_ready())
        {
            continue;
        }

        /* SAFE + 1 буфер: при display_ready() блокировки здесь уже не будет */
        uint16_t* buf = display_get_draw_buffer();
        render_begin(&rc, buf, WIDTH, HEIGHT);

        render_clear(&rc, RGB16(9,19,9));
        render_grid(&rc, 20, 20, 40, RGB16(12,26,13));
        render_sine_wave(&rc, WIDTH*3, 100, 2.0f, 0, HEIGHT / 2, phase, RGB16(0,255,0));
        draw_string(&rc, 50, text1_y, L"Проверка кириллицы", RGB565(255, 255, 255));
        draw_string(&rc, 45, text2_y, L"Proverka latinyanskogo", RGB565(0, 0, 255));
        draw_string(&rc, 35, text3_y, L"1234567890!@#$%%^&*()", RGB565(255, 0, 0));

        text1_y += text1_dy;
        if (text1_y <= 0)
        {
            text1_y = 0;
            text1_dy = 1;
        }
        else if (text1_y >= (HEIGHT - TEXT_H))
        {
            text1_y = HEIGHT - TEXT_H;
            text1_dy = -1;
        }

        text2_y += text2_dy;
        if (text2_y <= 0)
        {
            text2_y = 0;
            text2_dy = 1;
        }
        else if (text2_y >= (HEIGHT - TEXT_H))
        {
            text2_y = HEIGHT - TEXT_H;
            text2_dy = -1;
        }

        text3_y += text3_dy;
        if (text3_y <= 0)
        {
            text3_y = 0;
            text3_dy = 1;
        }
        else if (text3_y >= (HEIGHT - TEXT_H))
        {
            text3_y = HEIGHT - TEXT_H;
            text3_dy = -1;
        }

        phase += 0.16f;
        if (phase > 6.2831853f)
        {
            phase -= 6.2831853f;
        }

        display_submit();
    }
}
