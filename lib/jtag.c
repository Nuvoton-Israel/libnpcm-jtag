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

static JTAG_Handler jtag_handler;

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

void DBG_log(unsigned int level, const char *format, ...)
{
	if (level < jtag_handler.loglevel)
		return;

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);
}

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

STATUS JTAG_set_clock_frequency(JTAG_Handler *jtag, unsigned int frequency)
{
	unsigned long req = JTAG_SIOCFREQ;

	printf("Set freq: %u\n", frequency);
	if (ioctl(jtag->handle, req, &frequency) < 0) {
		DBG_log(LEV_ERROR, "ioctl JTAG_SIOCFREQ failed");
		return ST_ERR;
	}
	jtag->frequency = frequency;
	return ST_OK;
}

STATUS JTAG_get_clock_frequency(JTAG_Handler *jtag, unsigned int *frequency)
{
	unsigned long req = JTAG_GIOCFREQ;

	if (ioctl(jtag->handle, req, frequency) < 0) {
		DBG_log(LEV_ERROR, "ioctl JTAG_SIOCFREQ failed");
		return ST_ERR;
	}
	//printf("Get freq: %u\n", *frequency);
	return ST_OK;
}

STATUS JTAG_set_mode(JTAG_Handler *jtag, unsigned int Mode)
{
	if (ioctl(jtag->handle, JTAG_SIOCMODE, &Mode) < 0) {
		DBG_log(LEV_ERROR, "ioctl JTAG_SIOCMODE failed");
		return ST_ERR;
	}
	jtag->mode = Mode;
	return ST_OK;
}

STATUS JTAG_run_test(JTAG_Handler *jtag, JtagStates tap_state, unsigned int tcks)
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

//
// Request the TAP to go to the target state
//
STATUS JTAG_set_tap_state(JTAG_Handler *jtag, JtagStates tap_state)
{
	struct jtag_tap_state tapstate;
	unsigned long req = JTAG_SIOCSTATE;

	if (jtag == NULL)
		return ST_ERR;

	tapstate.reset = 0;
	tapstate.tck = 0;
	tapstate.from = JTAG_STATE_CURRENT;
	tapstate.endstate = tap_state;

	if (ioctl(jtag->handle, req, &tapstate) < 0) {
		DBG_log(LEV_ERROR, "ioctl JTAG_SIOCSTATE failed");
		perror("set tap state");
		return ST_ERR;
	}

	// move the [soft] state to the requested tap state.
	jtag->tap_state = tap_state;
#if 0
	if ((tap_state == JtagRTI) || (tap_state == JtagPauDR))
		if (JTAG_wait_cycles(state, 5) != ST_OK)
			return ST_ERR;
#endif
	DBG_log(LEV_DEBUG, "TapState: %d", jtag->tap_state);
	return ST_OK;
}

STATUS JTAG_shift(JTAG_Handler *jtag, struct scan_xfer *scan_xfer, unsigned int type)
{
	struct jtag_xfer xfer;
	unsigned char tdio[TDI_DATA_SIZE];
#if UINTPTR_MAX == 0xffffffff
	__u64 ptr = (__u32)tdio;
#else
	__u64 ptr = (__u64)tdio;
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
int JTAG_set_jtag_trst(JTAG_Handler* handler, unsigned int active)
{
        unsigned int trst_active = active;

        if (ioctl(handler->handle, JTAG_SIOCTRST, &trst_active) < 0) {
                perror("JTAG_SIOCTRST");
                return -1;
        }
        return 0;
}
#endif

int JTAG_dr_scan(JTAG_Handler *jtag, int num_bits, const uint8_t *out_bits, uint8_t *in_bits,
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
		if (JTAG_shift(jtag, &scan_xfer, JTAG_SDR_XFER) != ST_OK) {
			DBG_log(LEV_ERROR, "ShftDR error");
			return -1;
		}
		if (in_bits)
			memcpy(in_bits+index, scan_xfer.tdo, scan_xfer.tdo_bytes);
		index += n;
	}
	return 0;
}

int JTAG_ir_scan(JTAG_Handler *jtag, int num_bits, const uint8_t *out_bits, uint8_t *in_bits,
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
	if (JTAG_shift(jtag, &scan_xfer, JTAG_SIR_XFER) != ST_OK) {
		DBG_log(LEV_ERROR, "ShftIR error");
		return -1;
	}
	if (in_bits)
		memcpy(in_bits, scan_xfer.tdo, scan_xfer.tdo_bytes);

	return 0;
}

int JTAG_open(char *jtag_dev, int frequency, int mode)
{
	memset(&jtag_handler, 0, sizeof(jtag_handler));

	jtag_handler.handle = open(jtag_dev, O_RDWR);
	if (jtag_handler.handle == -1) {
		perror("Can't open jtag device");
		goto err;
	}

	/* set frequency */
	if (frequency > 0) {
		if (JTAG_set_clock_frequency(&jtag_handler, frequency) != ST_OK) {
			fprintf(stderr, "Unable to set the frequency: %d\n", frequency);
		}
	} else {
		JTAG_get_clock_frequency(&jtag_handler, &jtag_handler.frequency);
	}

	/* set transfer mode */
	if (JTAG_set_mode(&jtag_handler, mode) != ST_OK) {
		fprintf(stderr, "Failed to set JTAG mode: %d\n", mode);
	}

	jtag_handler.tap_state = JtagTLR;
	jtag_handler.loglevel = LOG_LEVEL_INFO;

	return jtag_handler.handle;
err:
	return -1;
}

void JTAG_close(int handle)
{
	if (handle)
		close(handle);
}

void JTAG_reset_state(int handle)
{
	if (handle != jtag_handler.handle) {
		printf("invalid handle\n");
		return;
	}
	if ((JTAG_set_tap_state(&jtag_handler, JtagTLR) != ST_OK) ||
	    (JTAG_set_tap_state(&jtag_handler, JtagRTI) != ST_OK)) {
		printf("Failed to reset TAP state.\n");
	}
}

int JTAG_load_svf(int handle, char *svf_path, int step)
{
	struct  timeval start, end;
	unsigned long diff;
	int ret;

	if (handle != jtag_handler.handle) {
		printf("invalid handle\n");
		return -1;
	}
	jtag_handler.single_step = step;
	gettimeofday(&start,NULL);
	ret = handle_svf_command(&jtag_handler, svf_path);
	gettimeofday(&end,NULL);
	diff = 1000 * (end.tv_sec-start.tv_sec)+ (end.tv_usec-start.tv_usec) / 1000;
	printf("loading time is %ld ms\n",diff);
	//printf("total runtest time is %ld ms\n", total_runtest_time / 1000);

	return ret;
}

void JTAG_set_loglevel(int handle, unsigned int level)
{
	if (handle != jtag_handler.handle) {
		printf("invalid handle\n");
		return;
	}
	jtag_handler.loglevel = level;
}

int JTAG_send_command(int handle, uint8_t *command, uint32_t len)
{
	int ret;

	if (handle != jtag_handler.handle) {
		printf("invalid handle\n");
		return -1;
	}
	ret = JTAG_ir_scan(&jtag_handler, len, (const uint8_t *)command, NULL, JtagRTI);
	//if (ret) {
	//	printf("Send command error\n");
	//}

	return ret;
}

int JTAG_transfer_data(int handle, uint8_t *out, uint8_t *in, uint32_t bit_len)
{
	int byte_len = (bit_len + 7) / 8;
	uint8_t *tdi = NULL;
	int ret;

	if (handle != jtag_handler.handle) {
		printf("invalid handle\n");
		return -1;
	}
	if (!out) {
		tdi = malloc(byte_len);
		memset(tdi, 0, byte_len);
		if (!tdi) {
			printf("%s: malloc error\n", __func__);
			return -1;
		}
	}
	ret = JTAG_dr_scan(&jtag_handler, bit_len, (const uint8_t *)out ? out : tdi, (uint8_t *)in, JtagRTI);

	if (tdi)
		free(tdi);

	return ret;
}

void JTAG_runtest_idle(int handle, uint32_t tcks)
{
	if (handle != jtag_handler.handle) {
		printf("invalid handle\n");
		return;
	}
	JTAG_run_test(&jtag_handler, JtagRTI, tcks);
}
