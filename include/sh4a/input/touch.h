#ifndef _TOUCH_H
#define _TOUCH_H

#include <stdint.h>

struct touch_adc_sample {
	uint16_t channel_a;
	uint16_t channel_b;
	uint16_t channel_c;
	uint16_t channel_d;
};

void touch_adc_init(void);
int touch_adc_read(struct touch_adc_sample *sample);

#endif /* _TOUCH_H */
