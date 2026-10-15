#ifndef _TOUCH_H
#define _TOUCH_H

#include <stdint.h>

struct touch_adc_sample {
	uint16_t channel_a;
	uint16_t channel_b;
	uint16_t channel_c;
	uint16_t channel_d;
};

/* Uncalibrated 12-bit samples from the I2C touch controller at address 0x48. */
struct touch_i2c_sample {
	uint16_t x;
	uint16_t y;
};

void touch_adc_init(void);
int touch_adc_read(struct touch_adc_sample *sample);

/* Bounded polling implementation of the ROM's I2C touch read. */
int touch_i2c_read(struct touch_i2c_sample *sample);

#endif /* _TOUCH_H */
