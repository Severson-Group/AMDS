#ifndef ADC_H
#define ADC_H

#include <stdint.h>

void adc_init(void);

// Read the latest bits from the ADCs.
//
// Note that this is the RAW bits at the ADC output,
// so the user must massage this into a usable value
// for their application.
//
// This is a non-blocking function.
//
// The ADC sampling is happening in the background.
// This function is guaranteed to return valid data,
// but if it is called in the middle of an conversion,
// one sample old data will be returned.
void adc_latest_bits(uint16_t *output);

#endif // ADC_H
