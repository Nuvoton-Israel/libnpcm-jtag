/* Copyright (c) 2023, Nuvoton Corporation */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "../include/jtag.h"

struct jtagdev_priv {
	int frequency;
	int mode;
	int loglevel;
};

static struct jtagdev_priv jtag_priv = {
	.frequency = 0,
	.mode = JTAG_MODE_HW,
	.loglevel = LEV_INFO
};

static void jtagdev_process_args(JTAG_Handler *handler, struct jtag_args *args)
{
	int i;

	for (i = 0; i < args->num_args; i++) {
		if (i >= JTAG_MAX_ARGS)
			return;
		if (args->arg[i].id == ARG_FREQ)
			jtag_priv.frequency = args->arg[i].val;
		else if (args->arg[i].id == ARG_LOG_LEVEL)
			jtag_priv.loglevel = args->arg[i].val;
		else if (args->arg[i].id == ARG_MODE)
			jtag_priv.mode = args->arg[i].val;
	}
}

static int jtagdev_set_clock_frequency(JTAG_Handler *jtag, int frequency)
{
	unsigned long req = JTAG_SIOCFREQ;

	printf("jtagdev: Set freq %u\n", frequency);
	if (ioctl(jtag->handle, req, &frequency) < 0) {
		DBG_log(LEV_ERROR, "ioctl JTAG_SIOCFREQ failed");
		return ST_ERR;
	}
	jtag->frequency = frequency;
	return ST_OK;
}

static int jtagdev_get_clock_frequency(JTAG_Handler *jtag)
{
	unsigned long req = JTAG_GIOCFREQ;
	int frequency = 0;

	if (ioctl(jtag->handle, req, &frequency) < 0) {
		DBG_log(LEV_ERROR, "ioctl JTAG_SIOCFREQ failed");
		return ST_ERR;
	}
	return frequency;
}

static int jtagdev_set_mode(JTAG_Handler *jtag, int Mode)
{
	if (ioctl(jtag->handle, JTAG_SIOCMODE, &Mode) < 0) {
		DBG_log(LEV_ERROR, "ioctl JTAG_SIOCMODE failed");
		return ST_ERR;
	}
	return ST_OK;
}

static int jtagdev_run_tck(JTAG_Handler *jtag, int tap_state, int tcks)
{
#ifdef USE_LEGACY_IOCTL
	if (jtag == NULL)
		return ST_ERR;
	if (ioctl(jtag->handle, JTAG_RUNTEST, tcks) < 0) {
		perror("runtest ioctl");
		return ST_ERR;
	}
#else
	struct jtag_tap_state tapstate;

	if (jtag == NULL)
		return ST_ERR;
	tapstate.reset = 0;
	tapstate.tck = tcks;
	tapstate.from = JTAG_STATE_CURRENT;
	tapstate.endstate = tap_state;
	if (ioctl(jtag->handle, JTAG_SIOCSTATE, &tapstate) < 0) {
		perror("run test");
		return ST_ERR;
	}
#endif
	return ST_OK;
}

static int jtagdev_set_tap_state(JTAG_Handler *jtag, int tap_state)
{
	struct jtag_tap_state tapstate;
	unsigned long req = JTAG_SIOCSTATE;

	tapstate.reset = 0;
	tapstate.tck = 0;
	tapstate.from = JTAG_STATE_CURRENT;
	tapstate.endstate = tap_state;

	if (ioctl(jtag->handle, req, &tapstate) < 0) {
		DBG_log(LEV_ERROR, "ioctl JTAG_SIOCSTATE failed");
		perror("set tap state");
		return ST_ERR;
	}

	jtag->tap_state = tap_state;
#if 0
	if ((tap_state == JtagRTI) || (tap_state == JtagPauDR))
		if (JTAG_wait_cycles(state, 5) != ST_OK)
			return ST_ERR;
#endif
	DBG_log(LEV_DEBUG, "TapState: %d", jtag->tap_state);
	return ST_OK;
}

static int jtagdev_get_tap_state(JTAG_Handler *jtag)
{
	unsigned long req = JTAG_GIOCSTATUS;
	int state;

	if (jtag == NULL)
		return ST_ERR;

	if (ioctl(jtag->handle, req, &state) < 0) {
		DBG_log(LEV_ERROR, "ioctl JTAG_GIOCSTATUS failed");
		perror("get tap state");
		return ST_ERR;
	}
	jtag->tap_state = state;

	DBG_log(LEV_DEBUG, "TapState: %d", jtag->tap_state);
	return ST_OK;
}

static int jtagdev_shift(JTAG_Handler *jtag, struct scan_xfer *scan_xfer, unsigned int type)
{
	struct jtag_xfer xfer;
	unsigned char tdio[TDI_DATA_SIZE];
#if UINTPTR_MAX == 0xffffffff
	uint64_t ptr = (uint32_t)tdio;
#else
	uint64_t ptr = (uint64_t)tdio;
#endif
	memset(&xfer, 0, sizeof(xfer));
	xfer.from = JTAG_STATE_CURRENT;
	xfer.endstate = scan_xfer->end_tap_state;
	xfer.length = scan_xfer->length;
	xfer.type = type;
	xfer.direction = JTAG_READ_WRITE_XFER;
	xfer.tdio = ptr;
	memcpy(tdio, scan_xfer->tdi, scan_xfer->tdi_bytes);
	if (ioctl(jtag->handle, JTAG_IOCXFER, &xfer) < 0) {
		perror("jtag shift");
		return ST_ERR;
	}
	memcpy(scan_xfer->tdo, tdio, scan_xfer->tdo_bytes);

	return ST_OK;
}

#ifndef USE_LEGACY_IOCTL
static int jtagdev_set_trst(JTAG_Handler* jtag, unsigned int active)
{
        unsigned int trst_active = active;

        if (ioctl(jtag->handle, JTAG_SIOCTRST, &trst_active) < 0) {
                perror("JTAG_SIOCTRST");
                return -1;
        }
        return 0;
}
#endif

static int jtagdev_shift_dr(JTAG_Handler *jtag, int num_bits, const uint8_t *out_bits, uint8_t *in_bits,
	tap_state_t state)
{
	struct scan_xfer scan_xfer = {0};
	int remaining_bits = num_bits;
	int n, bits, index = 0;

	memset(scan_xfer.tdi, 0, sizeof(scan_xfer.tdi));
	JTAG_set_tap_state(jtag, JtagShfDR);
	while (remaining_bits > 0) {
		n = (remaining_bits / 8) > TDI_DATA_SIZE ? TDI_DATA_SIZE : (remaining_bits + 7) / 8;
		memcpy(scan_xfer.tdi, out_bits + index, n);

		bits = ((n * 8) > remaining_bits)? remaining_bits: (n * 8);
		remaining_bits -= bits;
		scan_xfer.length = bits;
		scan_xfer.tdi_bytes = n;
		scan_xfer.tdo_bytes = n;
		if (remaining_bits > 0)
			scan_xfer.end_tap_state = JtagShfDR;
		else
			scan_xfer.end_tap_state = state;
		if (jtagdev_shift(jtag, &scan_xfer, JTAG_SDR_XFER) != ST_OK) {
			DBG_log(LEV_ERROR, "ShftDR error");
			return -1;
		}
		if (in_bits)
			memcpy(in_bits+index, scan_xfer.tdo, scan_xfer.tdo_bytes);
		index += n;
	}
	return 0;
}

static int jtagdev_shift_ir(JTAG_Handler *jtag, int num_bits, const uint8_t *out_bits, uint8_t *in_bits,
	tap_state_t state)
{
	struct scan_xfer scan_xfer = {0};
	if (num_bits == 0)
		return -1;
	if ((num_bits + 7) / 8 > TDO_DATA_SIZE) {
		DBG_log(LEV_ERROR, "ir data len too long: %d bits", num_bits);
		return -1;
	}
	JTAG_set_tap_state(jtag, JtagShfIR);
	scan_xfer.length = num_bits;
	scan_xfer.tdi_bytes = (num_bits + 7) / 8;
	memcpy(scan_xfer.tdi, out_bits, scan_xfer.tdi_bytes);
	scan_xfer.tdo_bytes = scan_xfer.tdi_bytes;
	scan_xfer.end_tap_state = state;
	if (jtagdev_shift(jtag, &scan_xfer, JTAG_SIR_XFER) != ST_OK) {
		DBG_log(LEV_ERROR, "ShftIR error");
		return -1;
	}
	if (in_bits)
		memcpy(in_bits, scan_xfer.tdo, scan_xfer.tdo_bytes);

	return 0;
}

static int jtagdev_open(JTAG_Handler *handler, char *jtag_dev, struct jtag_args *args)
{
	int frequency;

	handler->handle = open(jtag_dev, O_RDWR);
	if (handler->handle < 0) {
		perror("Can't open jtag device");
		return -1;
	}

	jtagdev_process_args(handler, args);
	frequency = jtag_priv.frequency;

	/* Set frequency */
	if (frequency > 0) {
		if (jtagdev_set_clock_frequency(handler, frequency) != ST_OK) {
			fprintf(stderr, "Unable to set the frequency: %d\n", frequency);
		}
	}

	/* Set transfer mode */
	if (jtagdev_set_mode(handler, jtag_priv.mode) != ST_OK) {
		fprintf(stderr, "Failed to set JTAG mode: %d\n", jtag_priv.mode);
	}
	handler->loglevel = jtag_priv.loglevel;

	jtagdev_get_tap_state(handler);

	return 0;
}

static void jtagdev_close(JTAG_Handler *handler)
{
	close(handler->handle);
}

static int jtagdev_load_svf(JTAG_Handler *handler, char *svf_path, bool step)
{
	handler->single_step = step;
	return handle_svf_command(handler, svf_path);
}

static const struct jtag_ops jtag_dev_ops = {
	.open = jtagdev_open,
	.close = jtagdev_close,
	.set_state = jtagdev_set_tap_state,
	.set_freq = jtagdev_set_clock_frequency,
	.get_freq = jtagdev_get_clock_frequency,
	.run_tck = jtagdev_run_tck,
	.shift_dr = jtagdev_shift_dr,
	.shift_ir = jtagdev_shift_ir,
	.load_svf = jtagdev_load_svf,
};

JTAG_Handler jtag_dev_handler = {
	.name = "jtag_dev",
	.type = JTAG_INTF_DEV,
	.priv = &jtag_priv,
	.ops = &jtag_dev_ops,
};
