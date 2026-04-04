#pragma once

#include <stddef.h>
#include <stdint.h>
#include "hardware/irq.h"

/* Initializes transport and panel backend, and registers DMA IRQ callback. */
void display_transport_init(uint16_t width, uint16_t height, irq_handler_t dma_irq_handler);

/* Starts a single frame transfer via panel backend + DMA. */
void display_transport_start_frame(uint16_t width, uint16_t height, uint16_t* buffer, size_t pixel_count);

/* Acknowledges DMA IRQ and finalizes frame SPI transaction. */
void display_transport_complete_irq(void);
