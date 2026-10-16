#include <sh4a/input/accelerometer.h>

/* SH7724 IIC, copied from the ROM's 0x80042ADA transaction primitive. */
#define IIC_ICDR (*(volatile uint8_t *)0xA4470000u)
#define IIC_ICCR (*(volatile uint8_t *)0xA4470004u)
#define IIC_ICSR (*(volatile uint8_t *)0xA4470008u)
#define IIC_ICIC (*(volatile uint8_t *)0xA447000cu)
#define IIC_ICCL (*(volatile uint8_t *)0xA4470010u)
#define IIC_ICCH (*(volatile uint8_t *)0xA4470014u)
#define CPG_FRQCR (*(volatile uint32_t *)0xA4150000u)
#define IIC_TIMEOUT 100000u
#define ACCEL_I2C_WRITE 0x02u
#define ACCEL_I2C_READ  0x03u

static int iic_wait(uint8_t mask)
{
	uint32_t timeout = IIC_TIMEOUT;
	while ((IIC_ICSR & mask) == 0) if (--timeout == 0) return 0;
	return 1;
}

static void iic_enable(void)
{
	IIC_ICCR |= 0x80;
	if ((CPG_FRQCR & 0x0fu) == 3u) {
		IIC_ICCL = 0x12;
		IIC_ICCH = 0x0c;
	} else {
		IIC_ICCL = 0x25;
		IIC_ICCH = 0x19;
	}
	IIC_ICSR &= 0xf1;
}

static int iic_start(void)
{
	IIC_ICIC = 3;
	IIC_ICCR = 0x94;
	if (!iic_wait(1)) return 0;
	IIC_ICIC = 2;
	return 1;
}

/* mode is the ROM's FUN_80042AF2 mode: 0 normal, 1 final write,
 * 2 read address, 3 repeated-start register address. */
static int iic_send(uint8_t value, uint8_t mode)
{
	IIC_ICDR = value;
	if (mode == 1) IIC_ICCR = 0x90;
	else if (mode == 2) {
		IIC_ICCR = 0x81;
		IIC_ICSR &= (uint8_t)~2u;
		if (!iic_wait(0x0au)) return 0;
	} else if (mode == 3) IIC_ICCR = 0x94;
	IIC_ICSR &= (uint8_t)~2u;
	if (!iic_wait(0x0au)) return 0;
	if (mode == 3) {
		IIC_ICSR &= (uint8_t)~2u;
		IIC_ICIC = 2;
		if (!iic_wait(1)) return 0;
	}
	return 1;
}

static int iic_receive(uint8_t *value, int final_byte)
{
	if (final_byte) IIC_ICCR = 0xc0;
	IIC_ICSR &= (uint8_t)~2u;
	if (!iic_wait(1)) return 0;
	IIC_ICIC = 2;
	*value = IIC_ICDR;
	return final_byte || iic_wait(0x0au);
}

static void iic_stop(void)
{
	IIC_ICIC = 0;
	IIC_ICCL = 0;
	IIC_ICCH = 0;
	IIC_ICCR &= 0x7f;
}

static int accel_write(uint8_t reg, uint8_t value)
{
	iic_enable();
	if (!iic_start() || !iic_send(ACCEL_I2C_WRITE, 0) ||
		!iic_send(reg, 0) || !iic_send(value, 1)) {
		iic_stop(); return 0;
	}
	/* IIC_FinishWrite (ROM 0x80042B50) clears status bit 1 then waits for
	 * ICSR.4. Disabling IIC before this edge was truncating configuration
	 * writes, leaving the accelerometer at its reset defaults. */
	IIC_ICSR &= (uint8_t)~2u;
	if (!iic_wait(0x10u)) { iic_stop(); return 0; }
	iic_stop();
	return 1;
}

static int accel_read(uint8_t reg, uint8_t *data, uint8_t length)
{
	uint8_t i;
	iic_enable();
	if (!iic_start() || !iic_send(ACCEL_I2C_WRITE, 0) ||
		!iic_send(reg | 0x80u, 3) || !iic_send(ACCEL_I2C_READ, 2)) goto fail;
	for (i = 0; i < length; i++)
		if (!iic_receive(&data[i], i + 1u == length)) goto fail;
	iic_stop();
	return 1;
fail:
	iic_stop();
	return 0;
}

int accelerometer_init(void)
{
	/* Exact setup writes made by ROM Accelerometer_Initialize. */
	return accel_write(0x06, 0x00) && accel_write(0x0a, 0x50) &&
		accel_write(0x0b, 0x01) && accel_write(0x0c, 0x00) &&
		accel_write(0x0d, 0x00) && accel_write(0x0e, 0x00) &&
		accel_write(0x0f, 0x00) && accel_write(0x10, 0x00) &&
		accel_write(0x11, 0x00);
}

int accelerometer_read(struct accelerometer_sample *sample)
{
	uint8_t raw[3];
	if (!sample || !accel_read(0x00, raw, 3)) return 0;
	sample->x = (int8_t)raw[0];
	sample->y = (int8_t)raw[1];
	sample->z = (int8_t)raw[2];
	return 1;
}
