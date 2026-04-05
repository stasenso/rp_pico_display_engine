#include "pico/stdlib.h"
#include "display/display.h"
#include "display/render/context.h"
#include "Font/font_data.h"
#include "dht22.h"
#include <wchar.h>

#define WIDTH   320
#define HEIGHT  240
#define DHT22_PIN 2
#define DHT22_POLL_INTERVAL_US 2000000ULL

static inline void wait_for_irq(void)
{
    __asm volatile ("wfi");
}

static const wchar_t* dht22_status_text(dht22_status_t status)
{
    switch (status)
    {
        case DHT22_OK: return L"DHT: OK";
        case DHT22_TIMEOUT: return L"DHT: timeout";
        case DHT22_CHECKSUM_ERROR: return L"DHT: crc error";
        case DHT22_BUS_STUCK: return L"DHT: bus stuck";
        default: return L"DHT: unknown";
    }
}

int main()
{
    stdio_init_all();

    display_config_t cfg = {
        .width = WIDTH,
        .height = HEIGHT,
        .buffer_count = 1,
        .mode = DISPLAY_MODE_SAFE,
    };

    display_init(&cfg);

    render_ctx_t rc;
    const wchar_t* title = L"Thermometr";
    const wchar_t* subtitle = L"DHT22 data";
    wchar_t temp_text[24] = L"T: --.- C";
    wchar_t hum_text[24] = L"H: --.- %";
    const wchar_t* dht_status = L"DHT: init";
    bool dht_has_valid_data = false;
    int16_t dht_temp_x10 = 0;
    uint16_t dht_hum_x10 = 0;
    uint64_t next_dht_poll_us = time_us_64();

    dht22_t dht;
    dht22_init(&dht, pio0, 0, DHT22_PIN);

    while (1)
    {
        uint64_t now_us = time_us_64();
        if (now_us >= next_dht_poll_us)
        {
            dht22_status_t status = dht22_read(&dht, &dht_temp_x10, &dht_hum_x10);
            dht_status = dht22_status_text(status);
            if (status == DHT22_OK)
            {
                dht_has_valid_data = true;
            }
            next_dht_poll_us = now_us + DHT22_POLL_INTERVAL_US;
        }

        if (dht_has_valid_data)
        {
            uint16_t t_abs_x10 = (dht_temp_x10 < 0) ? (uint16_t)(-dht_temp_x10) : (uint16_t)dht_temp_x10;
            wchar_t sign = (dht_temp_x10 < 0) ? L'-' : L'+';
            (void)swprintf(temp_text, sizeof(temp_text) / sizeof(temp_text[0]), L"T: %lc%u.%u C", sign, (unsigned)(t_abs_x10 / 10u), (unsigned)(t_abs_x10 % 10u));
            (void)swprintf(hum_text, sizeof(hum_text) / sizeof(hum_text[0]), L"H: %u.%u %%", (unsigned)(dht_hum_x10 / 10u), (unsigned)(dht_hum_x10 % 10u));
        }

        uint16_t* buf = display_begin_paint_try();
        if (buf == NULL)
        {
            wait_for_irq();
            continue;
        }

        render_begin(&rc, buf, WIDTH, HEIGHT);
        render_clear(&rc, RGB16(8, 18, 8));
        draw_string(&rc, 16, 16, title, RGB565(255, 255, 255));
        draw_string(&rc, 16, 40, subtitle, RGB565(255, 220, 120));
        draw_string(&rc, 16, 72, temp_text, RGB565(255, 255, 255));
        draw_string(&rc, 16, 96, hum_text, RGB565(255, 255, 255));
        draw_string(&rc, 16, 120, dht_status, RGB565(255, 200, 0));

        bool submitted = display_end_paint();
        hard_assert(submitted);
    }
}
