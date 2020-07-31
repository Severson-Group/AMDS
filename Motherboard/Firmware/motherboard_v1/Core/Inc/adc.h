#ifndef ADC_H
#define ADC_H

#include <stdint.h>

void adc_init(void);

void adc_read_raw(uint16_t *out1, uint16_t *out2);
void adc_read_volts(float *out1, float *out2);

#endif // ADC_H
