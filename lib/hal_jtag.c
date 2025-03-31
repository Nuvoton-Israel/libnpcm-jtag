#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>
#include "../include/jtag.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

extern JTAG_Handler jtag_dev_handler;
extern JTAG_Handler jtag_mctp_handler;
static JTAG_Handler *jtag_handlers[] = {
	&jtag_dev_handler,
	&jtag_mctp_handler,
};

static int loglevel = LEV_INFO;
void DBG_log(unsigned int level, const char *format, ...)
{
	if (level < loglevel)
		return;

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);
}

static const struct name_mapping {
    enum tap_state symbol;
    const char *name;
} tap_name_mapping[] = {
    { TAP_RESET, "RESET", },
    { TAP_IDLE, "RUN/IDLE", },
    { TAP_DRSELECT, "DRSELECT", },
    { TAP_DRCAPTURE, "DRCAPTURE", },
    { TAP_DRSHIFT, "DRSHIFT", },
    { TAP_DREXIT1, "DREXIT1", },
    { TAP_DRPAUSE, "DRPAUSE", },
    { TAP_DREXIT2, "DREXIT2", },
    { TAP_DRUPDATE, "DRUPDATE", },
    { TAP_IRSELECT, "IRSELECT", },
    { TAP_IRCAPTURE, "IRCAPTURE", },
    { TAP_IRSHIFT, "IRSHIFT", },
    { TAP_IREXIT1, "IREXIT1", },
    { TAP_IRPAUSE, "IRPAUSE", },
    { TAP_IREXIT2, "IREXIT2", },
    { TAP_IRUPDATE, "IRUPDATE", },

    /* only for input:  accept standard SVF name */
    { TAP_IDLE, "IDLE", },
};

const char *tap_state_name(tap_state_t state)
{
    unsigned i;

    for (i = 0; i < ARRAY_SIZE(tap_name_mapping); i++) {
        if (tap_name_mapping[i].symbol == state)
            return tap_name_mapping[i].name;
    }
    return "???";
}

tap_state_t tap_state_by_name(const char *name)
{
    unsigned i;

    for (i = 0; i < ARRAY_SIZE(tap_name_mapping); i++) {
        /* be nice to the human */
        if (strcasecmp(name, tap_name_mapping[i].name) == 0)
            return tap_name_mapping[i].symbol;
    }
    /* not found */
    return TAP_INVALID;
}

static JTAG_Handler *get_handler(int type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(jtag_handlers); i++) {
		if (jtag_handlers[i]->type == type)
			return jtag_handlers[i];
	}

	return NULL;
}

int jtag_args_add(struct jtag_args *args, JTAG_ARG_ID id, int val)
{
	if (args->num_args == JTAG_MAX_ARGS)
		return -1;
	args->arg[args->num_args].id = id;
	args->arg[args->num_args].val = val;
	args->num_args++;

	return 0;
}

JTAG_Handler *JTAG_open(char *intf, struct jtag_args *args)
{
	JTAG_Handler *handler;
	int rc;

	if (!strcmp(intf, "mctp"))
		handler = get_handler(JTAG_INTF_MCTP);
	else if (!strncmp(intf, "/dev/", 4))
		handler = get_handler(JTAG_INTF_DEV);
	else
		return NULL;

	printf("%s: handler %s\n", __func__, handler->name);
	rc = handler->ops->open(handler, intf, args);
	if (rc < 0)
		return NULL;

	loglevel = handler->loglevel;

	return handler;
}

void JTAG_close(JTAG_Handler *handler)
{
	handler->ops->close(handler);
}

int JTAG_set_tap_state(JTAG_Handler *handler, int state)
{
	return handler->ops->set_state(handler, state);
}

void JTAG_reset_state(JTAG_Handler *handler)
{
	JTAG_set_tap_state(handler, JtagTLR);
	JTAG_set_tap_state(handler, JtagRTI);
}

int JTAG_load_svf(JTAG_Handler *handler, char *svf_path, bool single)
{
	handler->single_step = single;
	return handle_svf_command(handler, svf_path);
}

int JTAG_set_clock_frequency(JTAG_Handler *handler, int frequency)
{
	int ret = 0;

	if (handler->ops->set_freq)
		ret = handler->ops->set_freq(handler, frequency);

	return ret;
}

int JTAG_get_clock_frequency(JTAG_Handler *handler)
{
	int ret = 0;

	if (handler->ops->get_freq)
		ret = handler->ops->get_freq(handler);

	return ret;
}

int JTAG_run_test(JTAG_Handler *handler, int state, int tcks)
{
	int ret = 0;

	if (handler->ops->run_tck)
		ret = handler->ops->run_tck(handler, state, tcks);

	return ret;
}

int JTAG_dr_scan(JTAG_Handler *handler, int bits, const uint8_t *out, uint8_t *in, int state)
{
	return handler->ops->shift_dr(handler, bits, out, in, state);
}

int JTAG_ir_scan(JTAG_Handler *handler, int bits, const uint8_t *out, uint8_t *in, int state)
{
	return handler->ops->shift_ir(handler, bits, out, in, state);
}

int JTAG_send_command(JTAG_Handler *handler, uint8_t *command, uint32_t len)
{
	return JTAG_ir_scan(handler, len, (const uint8_t *)command, NULL, JtagRTI);
}

int JTAG_transfer_data(JTAG_Handler *handler, uint8_t *out, uint8_t *in, uint32_t bit_len)
{
	int byte_len = (bit_len + 7) / 8;
	uint8_t *tdi = NULL;
	int ret;

	if (!out) {
		tdi = malloc(byte_len);
		memset(tdi, 0, byte_len);
		if (!tdi) {
			printf("%s: malloc error\n", __func__);
			return -1;
		}
	}
	ret = JTAG_dr_scan(handler, bit_len, (const uint8_t *)out ? out : tdi, (uint8_t *)in, JtagRTI);

	if (tdi)
		free(tdi);

	return ret;
}

void JTAG_runtest_idle(JTAG_Handler *handler, uint32_t tcks)
{
	JTAG_run_test(handler, JtagRTI, tcks);
}
