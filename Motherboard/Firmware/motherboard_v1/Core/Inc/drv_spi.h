#ifndef DRV_SPI_H
#define DRV_SPI_H

#include <stdint.h>

void drv_spi_init(void);

void drv_spi5_rx(uint8_t *data, uint16_t size);

#endif // DRV_SPI_H
