#ifndef _ACCELEROMETER_H
#define _ACCELEROMETER_H

#include <stdint.h>

/* ROM32A's three signed 8-bit acceleration samples. */
struct accelerometer_sample {
	int8_t x;
	int8_t y;
	int8_t z;
};

/* Direct I2C implementation of ROM Accelerometer_Initialize. */
int accelerometer_init(void);
int accelerometer_read(struct accelerometer_sample *sample);

#endif /* _ACCELEROMETER_H */
