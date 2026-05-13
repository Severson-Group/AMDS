#ifndef DRV_LED_H
#define DRV_LED_H

#include <stdint.h>

#define DRV_LED1 (0x01)
#define DRV_LED2 (0x02)
#define DRV_LED3 (0x04)
#define DRV_LED4 (0x08)

#define DRV_LED_NUM_TOTAL (4)

void drv_led_init(void);

void drv_led_set(uint8_t pattern);
void drv_led_clear(void);
void drv_led_on(uint8_t pattern);
void drv_led_off(uint8_t pattern);
void drv_led_toggle(uint8_t pattern);
void drv_led_display(void);

#endif // DRV_LED_H
