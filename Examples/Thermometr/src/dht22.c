#include "dht22.h"

#include "dht22.pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

#define DHT22_START_LOW_US      1200u
#define DHT22_PREPARE_US        30u
#define DHT22_WORD_TIMEOUT_US   2000u

static bool wait_for_rx_word(PIO pio, uint sm, uint32_t* out, uint32_t timeout_us) {
    uint64_t deadline = time_us_64() + timeout_us;
    while (pio_sm_is_rx_fifo_empty(pio, sm)) {
        if (time_us_64() > deadline) {
            return false;
        }
    }
    *out = pio_sm_get(pio, sm);
    return true;
}

void dht22_init(dht22_t* dev, PIO pio, uint sm, uint pin) {
    dev->pio = pio;
    dev->sm = sm;
    dev->pin = pin;
    dev->offset = pio_add_program(pio, &dht22_program);

    pio_gpio_init(pio, pin);

    pio_sm_config c = dht22_program_get_default_config(dev->offset);
    sm_config_set_in_pins(&c, pin);
    sm_config_set_clkdiv(&c, (float)clock_get_hz(clk_sys) / 1000000.0f);
    sm_config_set_in_shift(&c, false, true, 8);

    pio_sm_init(pio, sm, dev->offset, &c);
    pio_sm_set_enabled(pio, sm, false);

    gpio_init(pin);
    gpio_pull_up(pin);
    gpio_set_dir(pin, GPIO_IN);
}

dht22_status_t dht22_read(dht22_t* dev, int16_t* temperature_x10, uint16_t* humidity_x10) {
    if (!gpio_get(dev->pin)) {
        return DHT22_BUS_STUCK;
    }

    gpio_set_dir(dev->pin, GPIO_OUT);
    gpio_put(dev->pin, 0);
    sleep_us(DHT22_START_LOW_US);

    gpio_set_dir(dev->pin, GPIO_IN);
    gpio_pull_up(dev->pin);
    sleep_us(DHT22_PREPARE_US);

    pio_sm_set_enabled(dev->pio, dev->sm, false);
    pio_sm_clear_fifos(dev->pio, dev->sm);
    pio_sm_restart(dev->pio, dev->sm);
    pio_sm_set_enabled(dev->pio, dev->sm, true);

    uint8_t data[5] = {0};
    for (uint i = 0; i < 5; ++i) {
        uint32_t word = 0;
        if (!wait_for_rx_word(dev->pio, dev->sm, &word, DHT22_WORD_TIMEOUT_US)) {
            pio_sm_set_enabled(dev->pio, dev->sm, false);
            return DHT22_TIMEOUT;
        }
        data[i] = (uint8_t)word;
    }

    pio_sm_set_enabled(dev->pio, dev->sm, false);

    uint8_t checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (checksum != data[4]) {
        return DHT22_CHECKSUM_ERROR;
    }

    uint16_t raw_h = (uint16_t)((data[0] << 8) | data[1]);
    uint16_t raw_t = (uint16_t)((data[2] << 8) | data[3]);

    *humidity_x10 = raw_h;
    if (raw_t & 0x8000u) {
        *temperature_x10 = -(int16_t)(raw_t & 0x7fffu);
    } else {
        *temperature_x10 = (int16_t)raw_t;
    }

    return DHT22_OK;
}
