#include "display_driver.h"

#include <stddef.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#if DISPLAY_TYPE == DISPLAY_TYPE_ST7789

static inline void display_send_command(uint8_t cmd)
{
    gpio_put(DISPLAY_PIN_DC, 0);
    gpio_put(DISPLAY_PIN_CS, 0);
    spi_write_blocking(DISPLAY_SPI_PORT, &cmd, 1);
    gpio_put(DISPLAY_PIN_CS, 1);
}

static inline void display_send_data_bytes(const uint8_t* data, size_t len)
{
    gpio_put(DISPLAY_PIN_DC, 1);
    gpio_put(DISPLAY_PIN_CS, 0);
    spi_write_blocking(DISPLAY_SPI_PORT, data, len);
    gpio_put(DISPLAY_PIN_CS, 1);
}

static inline void display_send_data_u8(uint8_t data)
{
    display_send_data_bytes(&data, 1);
}

static inline void st7789_set_window(uint16_t width, uint16_t height)
{
    uint16_t x_end = (width > 0) ? (uint16_t)(width - 1u) : 0u;
    uint16_t y_end = (height > 0) ? (uint16_t)(height - 1u) : 0u;
    uint8_t window[4];

    display_send_command(0x2A); /* CASET */
    window[0] = 0x00;
    window[1] = 0x00;
    window[2] = (uint8_t)(x_end >> 8);
    window[3] = (uint8_t)x_end;
    display_send_data_bytes(window, sizeof(window));

    display_send_command(0x2B); /* RASET */
    window[2] = (uint8_t)(y_end >> 8);
    window[3] = (uint8_t)y_end;
    display_send_data_bytes(window, sizeof(window));
}

void display_driver_panel_init(uint16_t width, uint16_t height)
{
    gpio_set_function(DISPLAY_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(DISPLAY_PIN_SCK, GPIO_FUNC_SPI);

    gpio_init(DISPLAY_PIN_CS);
    gpio_init(DISPLAY_PIN_DC);
    gpio_init(DISPLAY_PIN_RST);
    gpio_init(DISPLAY_PIN_BL);
    gpio_set_dir(DISPLAY_PIN_CS, GPIO_OUT);
    gpio_set_dir(DISPLAY_PIN_DC, GPIO_OUT);
    gpio_set_dir(DISPLAY_PIN_RST, GPIO_OUT);
    gpio_set_dir(DISPLAY_PIN_BL, GPIO_OUT);

    gpio_put(DISPLAY_PIN_CS, 1);
    gpio_put(DISPLAY_PIN_DC, 1);
    gpio_put(DISPLAY_PIN_BL, 0);

    gpio_put(DISPLAY_PIN_RST, 0);
    sleep_ms(50);
    gpio_put(DISPLAY_PIN_RST, 1);
    sleep_ms(50);

    display_send_command(0x01); /* SWRESET */
    sleep_ms(150);
    display_send_command(0x11); /* SLPOUT */
    sleep_ms(150);

    display_send_command(0x36); /* MADCTL */
    display_send_data_u8(0b10100000);

    display_send_command(0x3A); /* COLMOD */
    display_send_data_u8(0x55); /* RGB565 */

    st7789_set_window(width, height);

    display_send_command(0x21); /* INVON */
    display_send_command(0x29); /* DISPON */
    gpio_put(DISPLAY_PIN_BL, 1);
}

void display_driver_begin_frame_transfer(uint16_t width, uint16_t height)
{
    st7789_set_window(width, height);
    display_send_command(0x2C); /* RAMWR */

    gpio_put(DISPLAY_PIN_DC, 1);
    gpio_put(DISPLAY_PIN_CS, 0);
}

void display_driver_end_frame_transfer(void)
{
    gpio_put(DISPLAY_PIN_CS, 1);
}

#else
#error "Unsupported DISPLAY_TYPE. Add implementation in src/core/display_driver.c."
#endif
