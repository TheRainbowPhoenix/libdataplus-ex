#include <sh4a/adc.h>
#include <sh4a/input/touch.h>

#define PFC_PSELH   (*(volatile uint16_t *)0xA4050118u)
#define PFC_PGCR    (*(volatile uint16_t *)0xA4050124u)
#define PFC_MSELCRA (*(volatile uint16_t *)0xA4050180u)
#define ADC_ADCCSR  (*(volatile uint16_t *)0xA4610088u)
#define ADC_ADPCTL  (*(volatile uint16_t *)0xA461008cu)
#define CPG_FRQCR   (*(volatile uint32_t *)0xA4150000u)
#define PORT_PLDR   (*(volatile uint8_t *)0xA4050134u)

/* SH7724 IIC: byte-wide registers, four bytes apart. */
#define IIC_ICDR (*(volatile uint8_t *)0xA4470000u)
#define IIC_ICCR (*(volatile uint8_t *)0xA4470004u)
#define IIC_ICSR (*(volatile uint8_t *)0xA4470008u)
#define IIC_ICIC (*(volatile uint8_t *)0xA447000cu)
#define IIC_ICCL (*(volatile uint8_t *)0xA4470010u)
#define IIC_ICCH (*(volatile uint8_t *)0xA4470014u)
#define IIC_TIMEOUT 100000u

void touch_adc_init(void)
{
	PFC_PSELH = (uint16_t)((PFC_PSELH & 0x0fffu) | 0x5000u);
	PFC_PGCR = 0;
	PFC_MSELCRA &= 0xfcffu;
	ADC_ADCCSR &= (uint16_t)~0x2000u;
	ADC_ADCCSR = 0x01cfu;
	ADC_ADPCTL = 0x8000u;
}

int touch_adc_read(struct touch_adc_sample *sample)
{
	struct adc raw;
	int ab_error;
	int cd_error;
	if (!sample)
		return 0;
	adc_read_multi(&raw);
	sample->channel_a = raw.channel_a;
	sample->channel_b = raw.channel_b;
	sample->channel_c = raw.channel_c;
	sample->channel_d = raw.channel_d;
	ab_error = (int)raw.channel_a - (0x3ff - (int)raw.channel_b);
	cd_error = (int)raw.channel_c - (0x3ff - (int)raw.channel_d);
	if (ab_error < 0) ab_error = -ab_error;
	if (cd_error < 0) cd_error = -cd_error;
	return ab_error < 0x41 && cd_error < 0x41;
}

static int iic_wait(uint8_t mask)
{
	uint32_t timeout = IIC_TIMEOUT;
	while ((IIC_ICSR & mask) == 0) if (--timeout == 0) return 0;
	return 1;
}

/* ROM32A 0x80042ADA..0x80042D24, with scheduler waits replaced by timeouts. */
static void iic_enable(void)
{
	IIC_ICCR |= 0x80;
	/* ROM IIC_ConfigureClock: FRQCR mode 3 uses 12/0c; every other
	 * mode uses 25/19. The old fixed 12/0c timing corrupted transfers. */
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

static int iic_transfer(uint8_t value, uint8_t control)
{
	IIC_ICDR = value;
	if (control) IIC_ICCR = control;
	/* ROM FUN_80042AF2 performs an additional completion wait after the
	 * read-address phase (ICCR=0x81). Missing it leaves ICDR stale. */
	if (control == 0x81u) {
		IIC_ICSR &= (uint8_t)~2u;
		if (!iic_wait(0x0au)) return 0;
	}
	IIC_ICSR &= (uint8_t)~2u;
	return iic_wait(0x0au);
}

static void iic_stop(void)
{
	IIC_ICIC = 0;
	IIC_ICCL = 0;
	IIC_ICCH = 0;
	IIC_ICCR &= 0x7f;
}

static int touch_i2c_command(uint8_t command)
{
	iic_enable();
	if (!iic_start() || !iic_transfer(0x90, 0) || !iic_transfer(command, 0x90)) {
		iic_stop();
		return 0;
	}
	IIC_ICSR &= (uint8_t)~2u;
	(void)iic_wait(0x10);
	iic_stop();
	return 1;
}

static int touch_i2c_read_word(uint16_t *word)
{
	uint8_t high, low;
	iic_enable();
	if (!iic_start() || !iic_transfer(0x91, 0x81)) goto fail;
	IIC_ICSR &= (uint8_t)~2u;
	if (!iic_wait(1)) goto fail;
	IIC_ICIC = 2;
	high = IIC_ICDR;
	if (!iic_wait(0x0au)) goto fail;
	IIC_ICCR = 0xc0;
	IIC_ICSR &= (uint8_t)~2u;
	if (!iic_wait(1)) goto fail;
	IIC_ICIC = 2;
	low = IIC_ICDR;
	iic_stop();
	*word = ((uint16_t)high << 8) | low;
	return 1;
fail:
	iic_stop();
	return 0;
}

static int touch_i2c_axis(uint8_t command, uint16_t minimum, uint16_t maximum, uint16_t *result)
{
	uint16_t values[5], discard, filtered = 0x8000u;
	uint8_t i, lowest = 0, highest = 0;
	if (!touch_i2c_command(command)) return 0;
	/* ROM 0x800825e2 throws away one conversion, then retains five. */
	if (!touch_i2c_read_word(&discard)) return 0;
	for (i = 0; i < 5; i++) {
		if (!touch_i2c_read_word(&values[i])) return 0;
		values[i] >>= 4;
		if (values[i] < values[lowest]) lowest = i;
		if (values[i] > values[highest]) highest = i;
	}
	for (i = 0; i < 5; i++) {
		if (i == lowest || i == highest) continue;
		if (values[i] < minimum || values[i] > maximum) return 0;
		/* Match the ROM's running pairwise average, not a six-sample mean. */
		if (filtered == 0x8000u) filtered = values[i];
		else filtered = (uint16_t)((filtered + values[i]) >> 1);
	}
	*result = filtered;
	return 1;
}

int touch_i2c_read(struct touch_i2c_sample *sample)
{
	uint16_t x, y;
	if (!sample) return 0;
	/* ROM commands: C4 => X, D4 => Y, then C0 cleanup. */
	/* The ROM accepts a point only while the active-low pen input remains low
	 * across five reads. Without this guard idle bus noise looked like a touch. */
	for (uint8_t pen = 0; pen < 5; pen++) if (PORT_PLDR & 1u) return 0;
	if (!touch_i2c_axis(0xc4, 0x0050, 0x0ff5, &x) ||
		!touch_i2c_axis(0xd4, 0x0070, 0x0ff1, &y)) return 0;
	(void)touch_i2c_command(0xc0);
	sample->x = x;
	sample->y = y;
	return 1;
}
