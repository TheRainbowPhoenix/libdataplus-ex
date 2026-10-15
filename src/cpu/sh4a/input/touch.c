#include <sh4a/adc.h>
#include <sh4a/input/touch.h>

#define PFC_PSELH   (*(volatile uint16_t *)0xA4050118u)
#define PFC_PGCR    (*(volatile uint16_t *)0xA4050124u)
#define PFC_MSELCRA (*(volatile uint16_t *)0xA4050180u)
#define ADC_ADCCSR  (*(volatile uint16_t *)0xA4610088u)
#define ADC_ADPCTL  (*(volatile uint16_t *)0xA461008cu)

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
