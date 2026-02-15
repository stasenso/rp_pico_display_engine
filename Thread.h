#ifndef THREADS_H
#define THREADS_H

#include <stdbool.h>
#include <stdio.h>
#include "hardware/timer.h"
#include "hardware/dma.h"



#define SPI_PORT spi0
#define PIN_MISO -1
#define PIN_MOSI 19
#define PIN_SCK  18
#define PIN_CS   17
#define PIN_DC   22
#define PIN_RST  13
#define PIN_BL   12
#define DHT_PIN 2 // GPIO для подключения DHT22

extern struct repeating_timer timer;


bool repeating_timer_callback(struct repeating_timer *t);
void st7789_send_command(uint8_t cmd);
void st7789_send_data(uint8_t data);
void st7789_send_framebuffer(uint16_t *buffer);
void coreEntry();
void st7789_init();
void dma_handler(); //Обработчик завершения DMA
void send_start_signal();
bool wait_for_signal(uint32_t timeout_us, bool level);
void read_dht_data(uint8_t *data);
bool dht_read(float *humidity, float *temperature);

#endif