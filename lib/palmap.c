
#include "ch.h"
#include "rhal.h"
#include "palmap.h"

#define ADC_NUM			8


static int adc_sample_to_pin[ADC_NUM];
static size_t adc_sample_len = 0;
static EVENTSOURCE_DECL(adc_done_es);
static systime_t adc_sample_rate = MS2ST(20);

#if HAL_USE_ADC

static void adc_cb(ADCDriver *adcp, adcsample_t *buffer, size_t n);

static adcsample_t adc_samples[ADC_NUM];
static VirtualTimer vtmr;

static ADCConversionGroup adc_grp = {
	FALSE,
	0,
	adc_cb,
	NULL,
	/* HW dependent part */
	0,
	ADC_CR2_TSVREFE,
	ADC_SMPR1_SMP_AN10(ADC_SAMPLE_41P5) |
		ADC_SMPR1_SMP_AN11(ADC_SAMPLE_41P5) |
		ADC_SMPR1_SMP_AN12(ADC_SAMPLE_41P5) |
		ADC_SMPR1_SMP_AN13(ADC_SAMPLE_41P5),
	ADC_SMPR2_SMP_AN0(ADC_SAMPLE_41P5) |
		ADC_SMPR2_SMP_AN1(ADC_SAMPLE_41P5) |
		ADC_SMPR2_SMP_AN2(ADC_SAMPLE_41P5) |
		ADC_SMPR2_SMP_AN3(ADC_SAMPLE_41P5),
	0,
	0,
	0
};

static void vtmr_func(void *p)
{
	(void)p;
	chSysLockFromIsr();
	adcStartConversionI(&ADCD1, &adc_grp, adc_samples, 1);
	chSysUnlockFromIsr();
}

static void adc_cb(ADCDriver *adcp, adcsample_t *buffer, size_t n)
{
	(void)buffer;
	(void)n;
	if (adcp->state == ADC_COMPLETE) {
		chSysLockFromIsr();
		if (!chVTIsArmedI(&vtmr))
			chVTSetI(&vtmr, adc_sample_rate, vtmr_func, NULL);
		chEvtBroadcastI(&adc_done_es);
		chSysUnlockFromIsr();
	}
}

#endif /* HAL_USE_ADC */

/*
 * Public API
 */

int pmGetPortOffset(char port_name)
{
	size_t i;

	for (i = 0; i < PALMAP_PORTS_SIZE; i++) {
		if (port_name == palmap_ports[i].port_name)
			return palmap_ports[i].pad_offset;
	}

	return -1;
}

int pmPortToPin(char port_name, int pin)
{
	int r = pmGetPortOffset(port_name);

	if (r == -1)
		return r;

	r += pin;
	return (r < PALMAP_PADS_SIZE)? r : -1;
}

int pmAnalogStart(int pin, bool_t state)
{
	size_t i;
	bool_t update_grp = FALSE;

	/* TODO: mutex */

	if (pmGetAdcChannel(pin) == -1)
		return -1;

	for (i = 0; i < adc_sample_len; i++) {
		if (adc_sample_to_pin[i] == pin)
			break;
	}

	if (i == adc_sample_len && state) {
		/* enable channel */
		adc_sample_to_pin[adc_sample_len++] = pin;

		update_grp = TRUE;
	}
	else if (!state) {
		/* disable channel */
		adc_sample_to_pin[i] = -1;
		adc_sample_len = 0;

		for (i = 0; i < ADC_NUM; i++) {
			if (adc_sample_to_pin[i] != -1)
				adc_sample_to_pin[adc_sample_len++] =
					adc_sample_to_pin[i];
		}

		update_grp = TRUE;
	}

	if (update_grp) {
		/* mark unused */
		for (i = adc_sample_len; i < ADC_NUM; i++)
			adc_sample_to_pin[i] = -1;

#if HAL_USE_ADC

		chVTReset(&vtmr);
		adcStopConversion(&ADCD1);

#define ADCH(pin) ((pin != -1)? pmGetAdcChannel(pin) : ADC_CHANNEL_IN0)

		adc_grp.num_channels = adc_sample_len;
		adc_grp.sqr1 = ADC_SQR1_NUM_CH(adc_sample_len);
		adc_grp.sqr2 = ADC_SQR2_SQ7_N(ADCH(adc_sample_to_pin[6])) |
			ADC_SQR2_SQ8_N(ADCH(adc_sample_to_pin[7]));
		adc_grp.sqr3 = ADC_SQR3_SQ1_N(ADCH(adc_sample_to_pin[0])) |
			ADC_SQR3_SQ2_N(ADCH(adc_sample_to_pin[1])) |
			ADC_SQR3_SQ3_N(ADCH(adc_sample_to_pin[2])) |
			ADC_SQR3_SQ4_N(ADCH(adc_sample_to_pin[3])) |
			ADC_SQR3_SQ5_N(ADCH(adc_sample_to_pin[4])) |
			ADC_SQR3_SQ6_N(ADCH(adc_sample_to_pin[5]));

#undef ADCH

		if (adc_sample_len)
			adcStartConversion(&ADCD1, &adc_grp, adc_samples, 1);

#endif /* HAL_USE_ADC */
	}

	return 0;
}

int pmAnalogReadLast(int pin)
{
	size_t i;

#if HAL_USE_ADC

	for (i = 0; i < adc_sample_len; i++) {
		if (adc_sample_to_pin[i] == pin)
			return adc_samples[i];
	}

#endif /* HAL_USE_ADC */

	return -1;
}

void pmAnalogSetRate(int rate_ms)
{
	adc_sample_rate = MS2ST(rate_ms);
}

EventSource *pmAnalogDoneEvent(void)
{
	return &adc_done_es;
}

