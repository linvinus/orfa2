
#include <string.h>

#include "eterm.h"
#include "palmap.h"

#define BUFSZ	32
#define ADCSZ	8

enum pin_mode {
	IN,
	OUT,
	ADC
};

static char buf[BUFSZ];
static char *p;

static bool_t adc_raw_out = FALSE;
static enum pin_mode pin_modes[PALMAP_PADS_SIZE];
static adcsample_t adc_samples[ADCSZ];
static uint16_t adc_pad[ADCSZ];


static inline int topin(char port, char pin)
{
	int p;

	if (pin < '0' || pin > '8')
		return -1;

	p = palmapGetPortOffset(port);
	if (p == -1)
		return -1;

	p += pin - '0';
	return (p < PALMAP_PADS_SIZE)? p : -1;
}

static void pin_mode(BaseChannel *chp, char port_, char pin_, char mode_)
{
	char *mode_str;
	int pin, pal_mode;

	pin = topin(port_, pin_);
	if (pin == -1) {
		chprintf(chp, "ERROR: unknown pin %c%c\n", port_, pin_);
		return;
	}

	switch (mode_) {
		case 'A':
			if (palmapGetAdcChannel(pin) != -1) {
				mode_str = "ADC";
				pin_modes[pin] = ADC;
				pal_mode = PAL_MODE_INPUT_ANALOG;
			} else {
				chprintf(chp, "ERROR: not ADC\n");
				return;
			}
			break;

		case 'O':
			mode_str = "Out";
			pin_modes[pin] = OUT;
			pal_mode = PAL_MODE_OUTPUT_PUSHPULL;
			break;

		case 'I':
		default:
			mode_str = "In";
			pin_modes[pin] = IN;
			pal_mode = PAL_MODE_INPUT_PULLUP;
			break;
	};

	palSetPadMode(palmapGetPortId(pin), palmapGetPad(pin), pal_mode);

	chprintf(chp, "PinMode%c%c=%s\n", port_, pin_, mode_str);
}

static void pin_get(BaseChannel *chp, char port_, char pin_)
{
	int pin;

	pin = topin(port_, pin_);
	if (pin == -1) {
		chprintf(chp, "ERROR: unknown pin %c%c\n", port_, pin_);
		return;
	}

	if (pin_modes[pin] == ADC) {
		// TODO
		chprintf(chp, "%c%c:%d.%d\n", port_, pin_, 0, 0);
	}
	else {
		chprintf(chp, "%c%c:%c\n", port_, pin_,
			palReadPad(palmapGetPortId(pin), palmapGetPad(pin))? '1' : '0' );
	}
}

static void pin_set(BaseChannel *chp, char port_, char pin_, char st)
{
	int pin;

	st = st != '0';
	pin = topin(port_, pin_);
	if (pin == -1) {
		chprintf(chp, "ERROR: unknown pin %c%c\n", port_, pin_);
		return;
	}

	if (pin_modes[pin] != OUT) {
		chprintf(chp, "ERROR: not an output\n");
		return;
	}

	palWritePad(palmapGetPortId(pin), palmapGetPad(pin), st);
	chprintf(chp, "%c%c=%d\n", port_, pin_, st);
}

static bool_t pin_adc_io_cb(BaseChannel *chp, char c, bool_t reinit)
{
	if (reinit) {
		p = buf;
		*p++ = c;
		return FALSE;
	}

	/* read to buffer */
	if (c != '\n') {
		if (p < buf + BUFSZ)
			*p++ = c;

		return FALSE;
	}
	p = '\0'; /* terminate buffer */

	/* scan buffer */
	if (strncmp(buf, "PinMode", 7) == 0) {
		/* PinModeX0=Y */
		pin_mode(chp, buf[7], buf[8], buf[10]);
	}
	else if (strncmp(buf, "PinGet", 6) == 0) {
		/* PinGetX0 */
		pin_get(chp, buf[6], buf[7]);
	}
	else if (strncmp(buf, "PinSet", 6) == 0) {
		/* PinSetX0=Y */
		pin_set(chp, buf[6], buf[7], buf[9]);
	}
	else if (strncmp(buf, "AdcBits", 7) == 0) {
		/* compat */
		chprintf(chp, "AdcBits=12\n");
	}
	else if (strncmp(buf, "AdcRef", 6) == 0) {
		/* compat */
		chprintf(chp, "AdcRef=Int\n");
	}
	else if (strncmp(buf, "AdcRAW", 6) == 0) {
		adc_raw_out = buf[7] == '1';
		chprintf(chp, "AdcRAW=%d\n", adc_raw_out);
	}
	else {
		chprintf(chp, "ERROR: unknown io command \"%s\"\n", buf);
	}

	return TRUE;
}

static eterm_node_t io_nodes[] = {
	ETERM_INIT('P', "Pin control", pin_adc_io_cb),
	ETERM_INIT('A', "ADC control", pin_adc_io_cb)
};

void eterm_init_io_nodes(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(io_nodes); i++)
		etermRegister(&io_nodes[i]);
}

