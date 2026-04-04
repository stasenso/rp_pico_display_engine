#pragma once

#include <stdint.h>

/* Display type identifiers used by DISPLAY_TYPE. */
#define DISPLAY_TYPE_ST7789 1
#define DISPLAY_TYPE_ILI9341 2

#ifndef DISPLAY_TYPE
#define DISPLAY_TYPE DISPLAY_TYPE_ST7789
#endif

/* Backward compatibility with old compile definitions from CMake. */
#if !defined(DISPLAY_SPI_PORT) && defined(SPI_PORT)
#define DISPLAY_SPI_PORT SPI_PORT
#endif
#ifndef DISPLAY_SPI_PORT
#define DISPLAY_SPI_PORT spi0
#endif

#if !defined(DISPLAY_PIN_MOSI) && defined(PIN_MOSI)
#define DISPLAY_PIN_MOSI PIN_MOSI
#endif
#ifndef DISPLAY_PIN_MOSI
#define DISPLAY_PIN_MOSI 19
#endif

#if !defined(DISPLAY_PIN_SCK) && defined(PIN_SCK)
#define DISPLAY_PIN_SCK PIN_SCK
#endif
#ifndef DISPLAY_PIN_SCK
#define DISPLAY_PIN_SCK 18
#endif

#if !defined(DISPLAY_PIN_CS) && defined(PIN_CS)
#define DISPLAY_PIN_CS PIN_CS
#endif
#ifndef DISPLAY_PIN_CS
#define DISPLAY_PIN_CS 17
#endif

#if !defined(DISPLAY_PIN_DC) && defined(PIN_DC)
#define DISPLAY_PIN_DC PIN_DC
#endif
#ifndef DISPLAY_PIN_DC
#define DISPLAY_PIN_DC 22
#endif

#if !defined(DISPLAY_PIN_RST) && defined(PIN_RST)
#define DISPLAY_PIN_RST PIN_RST
#endif
#ifndef DISPLAY_PIN_RST
#define DISPLAY_PIN_RST 13
#endif

#if !defined(DISPLAY_PIN_BL) && defined(PIN_BL)
#define DISPLAY_PIN_BL PIN_BL
#endif
#ifndef DISPLAY_PIN_BL
#define DISPLAY_PIN_BL 12
#endif

/* Display controller abstraction, selected by DISPLAY_TYPE define. */
void display_driver_panel_init(uint16_t width, uint16_t height);
void display_driver_begin_frame_transfer(uint16_t width, uint16_t height);
void display_driver_end_frame_transfer(void);
