#ifndef DRV_SPI_H
#define DRV_SPI_H

#include "platform.h"
#include <stdint.h>

void drv_spi_init(void);

static inline void drv_spi_start_read_two_16bits(SPI_TypeDef *spi)
{
    // Writing to DR triggers dummy data to be sent,
    // thus driving SCK. This loads our actual data.
    spi->DR = 0x12345678;

    // When we write to DR, the peripheral actually writes
    // to a TX FIFO. So, we can write to it again to queue
    // up another 16 clocks! This is an optimization which
    // masks the APB latency between ADC reads. :)
    spi->DR = 0x12345678;
}

static inline void drv_spi_wait_for_RX(SPI_TypeDef *spi)
{
    // Wait until we have received at least one word
    while (!(spi->SR & SPI_SR_RXNE)) {
    }
}

static inline void drv_spi_get_DR(SPI_TypeDef *spi, uint16_t *out)
{
    *out = (uint16_t) spi->DR;
}

static inline void drv_spi_finish_read_one_16bits(SPI_TypeDef *spi, uint16_t *out)
{
    drv_spi_wait_for_RX(spi);
    drv_spi_get_DR(spi, out);
}

static inline void drv_spi_finish_read_two_16bits(SPI_TypeDef *spi, uint16_t *out1, uint16_t *out2)
{
    drv_spi_wait_for_RX(spi);
    drv_spi_get_DR(spi, out1);

    drv_spi_wait_for_RX(spi);
    drv_spi_get_DR(spi, out2);
}

static inline void drv_spi_read_two_16bits(SPI_TypeDef *spi, uint16_t *out1, uint16_t *out2)
{
    drv_spi_start_read_two_16bits(spi);
    drv_spi_finish_read_two_16bits(spi, out1, out2);
}

#endif // DRV_SPI_H
