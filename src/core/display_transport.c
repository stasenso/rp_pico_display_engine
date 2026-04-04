#include "display_transport.h"
#include "display_driver.h"

#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

static int dma_chan = -1;

void display_transport_init(uint16_t width, uint16_t height, irq_handler_t dma_irq_handler)
{
    spi_init(DISPLAY_SPI_PORT, 1000 * 100 * 625); /* 62.5 MHz */
    spi_set_format(
        DISPLAY_SPI_PORT,
        8,
        SPI_CPOL_0,
        SPI_CPHA_0,
        SPI_MSB_FIRST
    );

    display_driver_panel_init(width, height);

    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config((uint)dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, spi_get_dreq(DISPLAY_SPI_PORT, true));

    dma_channel_configure(
        (uint)dma_chan,
        &cfg,
        &spi_get_hw(DISPLAY_SPI_PORT)->dr,
        NULL,
        0,
        false
    );

    dma_channel_set_irq0_enabled((uint)dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

void display_transport_start_frame(uint16_t width, uint16_t height, uint16_t* buffer, size_t pixel_count)
{
    display_driver_begin_frame_transfer(width, height);
    dma_channel_set_read_addr((uint)dma_chan, buffer, false);
    dma_channel_set_trans_count((uint)dma_chan, pixel_count * sizeof(uint16_t), true);
}

void display_transport_complete_irq(void)
{
    if (dma_chan >= 0)
    {
        dma_hw->ints0 = 1u << (uint)dma_chan;
    }

    display_driver_end_frame_transfer();
}
