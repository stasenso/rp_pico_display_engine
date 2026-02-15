#include "Thread.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pico/multicore.h"
#include "BackBuffer.h"
static int dma_chan;
static volatile bool dma_busy = false;

struct repeating_timer timer;
//uint16_t frame_buffer1[WIDTH * HEIGHT]; // Буфер для экрана1

void coreEntry(){
    st7789_init(); // Initialize SPI and GPIO
    gpio_put(PIN_BL, 1); // Подсветка
    
    gpio_pull_up(DHT_PIN);
    
    if (!add_repeating_timer_ms(2000, repeating_timer_callback, NULL, &timer)) {
    printf("Failed to add timer\n");
    while (true); // Остановите выполнение, если ошибка
}

    multicore_fifo_push_blocking(0); //Экран 0 свободен
    //multicore_fifo_push_blocking(1); //Экран 1 свободен

    while (true)
    {
        uint32_t data = multicore_fifo_pop_blocking();
        if (data==0){
            if (!dma_busy){
                st7789_send_framebuffer(frame_buffer);
            }
        }
        /*else if (data==1)
        {
            st7789_send_framebuffer(frame_buffer1); // Рисую первый экран
            multicore_fifo_push_blocking(1); //Экран 1 свободен
        }*/
        else
        {
            multicore_reset_core1(); // Сброс и остановка ядра 1
        }
        
    }
    

}

void st7789_init() {
    // Initialize SPI
    spi_init(SPI_PORT, 1000 * 100 * 625); // 62.5MHz
    spi_set_format(SPI_PORT,
               8,
               SPI_CPOL_0,
               SPI_CPHA_0,
               SPI_MSB_FIRST);

    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);

    // Initialize control pins
    gpio_init(PIN_CS);
    gpio_init(PIN_DC);
    gpio_init(PIN_RST);
    gpio_init(PIN_BL);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_RST, GPIO_OUT);
    gpio_set_dir(PIN_BL, GPIO_OUT);

    // Reset the display
    gpio_put(PIN_RST, 0);
    sleep_ms(50);
    gpio_put(PIN_RST, 1);
    sleep_ms(50);

    // Initialize ST7789 commands
    st7789_send_command(0x01); // Software reset
    sleep_ms(150);
    st7789_send_command(0x11); // Exit sleep mode
    sleep_ms(150);

    st7789_send_command(0x36); // Memory data access control 215
    st7789_send_data(0b10100000); // RGB
    //st7789_send_data(0b00101000); // BGR

    st7789_send_command(0x3A); // Interface pixel format
    st7789_send_data(0b01010101);    // 16-bit/pixel

       /*/ Настройка гаммы
    st7789_send_command(0x26); // Gamma set
    st7789_send_data(0x02); // Gamma curve 1 (User-defined)

    st7789_send_command(0xE0); // Positive gamma correction
    st7789_send_data(0xD0); // V0
    st7789_send_data(0x00); // V1
    st7789_send_data(0x02); // V2
    st7789_send_data(0x07); // V4
    st7789_send_data(0x0a); // V6
    st7789_send_data(0x29); // V13
    st7789_send_data(0x32); // V20
    st7789_send_data(0x44); // V36
    st7789_send_data(0x42); // V43
    st7789_send_data(0x06); // V50
    st7789_send_data(0x0e); // V57
    st7789_send_data(0x12); // V64
    st7789_send_data(0x14); // V71
    st7789_send_data(0x17); // V78

    st7789_send_command(0xE1); // Negative gamma correction
    st7789_send_data(0xD0); // V0
    st7789_send_data(0x00); // V1
    st7789_send_data(0x02); // V2
    st7789_send_data(0x07); // V4
    st7789_send_data(0x0a); // V6
    st7789_send_data(0x28); // V13
    st7789_send_data(0x31); // V20
    st7789_send_data(0x54); // V36
    st7789_send_data(0x47); // V43
    st7789_send_data(0x0e); // V50
    st7789_send_data(0x1c); // V57
    st7789_send_data(0x17); // V64
    st7789_send_data(0x1b); // V71
    st7789_send_data(0x1e); // V78*/

    // DMA init
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(SPI_PORT, true));

    dma_channel_configure(
        dma_chan,
        &c,
        &spi_get_hw(SPI_PORT)->dr,
        NULL,
        0,
        false
    );

    st7789_send_command(0x21); //INVON (21h): Display Inversion On
    st7789_send_command(0x29); // Display ON
}

void st7789_send_command(uint8_t cmd) {
    gpio_put(PIN_DC, 0);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(PIN_CS, 1);
}

void st7789_send_data(uint8_t data) {
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &data, 1);
    gpio_put(PIN_CS, 1);
}

void st7789_send_framebuffer(uint16_t *buffer) {
// Установить область для записи на весь экран
// Установка окна по X
    st7789_send_command(0x2A); // Column address set
    st7789_send_data(0x00);
    st7789_send_data(0x00);           // X-start = 0
    st7789_send_data((WIDTH - 1) >> 8);
    st7789_send_data((WIDTH - 1) & 0xFF); // X-end = WIDTH-1

// Установка окна по Y
    st7789_send_command(0x2B); // Row address set
    st7789_send_data(0x00);
    st7789_send_data(0x00);           // Y-start = 0
    st7789_send_data((HEIGHT - 1) >> 8);
    st7789_send_data((HEIGHT - 1) & 0xFF); // Y-end = HEIGHT-1

    st7789_send_command(0x2C); // Memory write. Команда записи данных в память дисплея

    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);

    dma_busy = true;

    dma_channel_set_read_addr(dma_chan, buffer, false);
    dma_channel_set_trans_count(dma_chan, WIDTH * HEIGHT *2, true);
}

void send_start_signal() {
    gpio_init(DHT_PIN);
    gpio_set_dir(DHT_PIN, GPIO_OUT);
    
    // Низкий уровень на 1 мс
    gpio_put(DHT_PIN, 0);
    sleep_ms(1);
    
    // Высокий уровень на 20-40 мкс
    gpio_put(DHT_PIN, 1);
    sleep_us(40);
}

bool wait_for_signal(uint32_t timeout_us, bool level) {
    uint32_t start_time = time_us_32();
    while (gpio_get(DHT_PIN) != level) {
        if (time_us_32() - start_time > timeout_us) {
            return false; // Таймаут
        }
    }
    return true;
}

void read_dht_data(uint8_t *data) {
    for (int i = 0; i < 40; i++) {
        // Ожидаем начала высокого уровня
        if (!wait_for_signal(80, 1)) return;

        // Замер длительности высокого уровня
        uint32_t start_time = time_us_32();
        if (!wait_for_signal(100, 0)) return;
        uint32_t pulse_length = time_us_32() - start_time;

        // Сохраняем бит (1, если > 50 мкс)
        data[i / 8] <<= 1;
        if (pulse_length > 50) {
            data[i / 8] |= 1;
        }
    }
}

bool dht_read(float *humidity, float *temperature) {
    uint8_t data[5] = {0};

    send_start_signal();
    gpio_set_dir(DHT_PIN, GPIO_IN);

    // Ожидаем ответа от датчика
    if (!wait_for_signal(100, 0)) return false; // Ждём низкого уровня
    if (!wait_for_signal(100, 1)) return false; // Ждём высокого уровня

    // Читаем данные
    read_dht_data(data);

    // Проверяем контрольную сумму
    if (data[4] != (data[0] + data[1] + data[2] + data[3])) return false;

    // Расчёт значений
    *humidity = ((data[0] << 8) | data[1]) / 10.0;
    *temperature = ((data[2] << 8) | data[3]) / 10.0;

    return true;
}

bool repeating_timer_callback(struct repeating_timer *t) {
    return true;
    if (dht_read(&humidity, &temperature)) {
    } else {
        humidity=-0.0;
        temperature=-0.0;
    }
}

void dma_handler() //Обработчик завершения DMA
{
    dma_hw->ints0 = 1u << dma_chan;
    dma_busy = false;
    gpio_put(PIN_CS, 1);
    multicore_fifo_push_blocking(0);  // <-- экран действительно свободен
}
