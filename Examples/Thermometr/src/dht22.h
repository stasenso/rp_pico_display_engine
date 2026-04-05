#ifndef DHT22_H
#define DHT22_H

#include "pico/types.h"
#include "hardware/pio.h"

typedef enum {
    DHT22_OK = 0,
    DHT22_TIMEOUT,
    DHT22_CHECKSUM_ERROR,
    DHT22_BUS_STUCK
} dht22_status_t;

typedef struct {
    PIO pio;
    uint sm;
    uint pin;
    uint offset;
} dht22_t;

void dht22_init(dht22_t* dev, PIO pio, uint sm, uint pin);
dht22_status_t dht22_read(dht22_t* dev, int16_t* temperature_x10, uint16_t* humidity_x10);

#endif
