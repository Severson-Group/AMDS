#ifndef ADC_H
#define ADC_H

#include <stdint.h>

void adc_init(void);
void try_read_sensors_before_trigger(void);

#endif // ADC_H
