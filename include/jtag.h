#ifndef __JTAG_H__
#define __JTAG_H__
#include <stdint.h>

#include "config.h"
#include "jtag_api.h"
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

#define LEV_DEBUG	1
#define LEV_INFO	2
#define LEV_ERROR	3

typedef uint8_t __u8;
typedef uint32_t __u32;
typedef uint64_t __u64;

typedef enum {
	JtagTLR,
	JtagRTI,
	JtagSelDR,
	JtagCapDR,
	JtagShfDR,
	JtagEx1DR,
	JtagPauDR,
	JtagEx2DR,
	JtagUpdDR,
	JtagSelIR,
	JtagCapIR,
	JtagShfIR,
	JtagEx1IR,
	JtagPauIR,
	JtagEx2IR,
	JtagUpdIR,
	JTAG_STATE_CURRENT
} JtagStates;

typedef enum tap_state {
    TAP_INVALID = -1,
    /* Proper ARM recommended numbers */
    TAP_DREXIT2 = JtagEx2DR,
    TAP_DREXIT1 = JtagEx1DR,
    TAP_DRSHIFT = JtagShfDR,
    TAP_DRPAUSE = JtagPauDR,
    TAP_IRSELECT = JtagSelIR,
    TAP_DRUPDATE = JtagUpdDR,
    TAP_DRCAPTURE = JtagCapDR,
    TAP_DRSELECT = JtagSelDR,
    TAP_IREXIT2 = JtagEx2IR,
    TAP_IREXIT1 = JtagEx1IR,
    TAP_IRSHIFT = JtagShfIR,
    TAP_IRPAUSE = JtagPauIR,
    TAP_IDLE = JtagRTI,
    TAP_IRUPDATE = JtagUpdIR,
    TAP_IRCAPTURE = JtagCapIR,
    TAP_RESET = JtagTLR,
} tap_state_t;


typedef enum {
	ST_OK,
	ST_ERR
} STATUS;

typedef enum {
    JTAGDriverState_Master = 0,
    JTAGDriverState_Slave
} JTAGDriverState;

typedef struct JTAG_Handler {
    int handle;
    JtagStates tap_state;
    int mode;
    unsigned int frequency;
    unsigned int loglevel;
    int single_step;
} JTAG_Handler;

#define TDI_DATA_SIZE	    256
#define TDO_DATA_SIZE	    256
#define JTAG_MAX_XFER_DATA_LEN 65535
#define MAX_DATA_SIZE 3000
struct scan_xfer {
	unsigned int     length;      // number of bits to clock
	unsigned char    tdi[TDI_DATA_SIZE];        // data to write to tap (optional)
	unsigned int     tdi_bytes;
	unsigned char    tdo[TDO_DATA_SIZE];        // data to read from tap (optional)
	unsigned int     tdo_bytes;
	unsigned int     end_tap_state;
};
struct jtag_xfer {
	__u8	type;
	__u8	direction;
	__u8	from;
	__u8	endstate;
	__u32	padding;
	__u32	length;
	__u64	tdio;
};

struct jtag_tap_state {
	__u8	reset;
	__u8	from;
	__u8	endstate;
	__u8	tck;
};

enum jtag_xfer_type {
	JTAG_SIR_XFER = 0,
	JTAG_SDR_XFER = 1,
};

enum jtag_xfer_direction {
	JTAG_READ_XFER = 1,
	JTAG_WRITE_XFER = 2,
	JTAG_READ_WRITE_XFER = 3,
};
struct scan_field {
    /** The number of bits this field specifies */
    int num_bits;
    /** A pointer to value to be scanned into the device */
    const uint8_t *out_value;
    /** A pointer to a 32-bit memory location for data scanned out */
    uint8_t *in_value;

    /** The value used to check the data scanned out. */
    uint8_t *check_value;
    /** The mask to go with check_value */
    uint8_t *check_mask;
};

#define __JTAG_IOCTL_MAGIC	0xb2
#define JTAG_SIOCSTATE	_IOW(__JTAG_IOCTL_MAGIC, 0, struct jtag_tap_state)
#define JTAG_SIOCFREQ	_IOW(__JTAG_IOCTL_MAGIC, 1, unsigned int)
#define JTAG_GIOCFREQ	_IOR(__JTAG_IOCTL_MAGIC, 2, unsigned int)
#define JTAG_IOCXFER	_IOWR(__JTAG_IOCTL_MAGIC, 3, struct jtag_xfer)
#define JTAG_GIOCSTATUS _IOWR(__JTAG_IOCTL_MAGIC, 4, JtagStates)
#define JTAG_SIOCMODE	_IOW(__JTAG_IOCTL_MAGIC, 5, unsigned int)
#define JTAG_IOCBITBANG	_IOW(__JTAG_IOCTL_MAGIC, 6, unsigned int)
#ifdef USE_LEGACY_IOCTL
#define JTAG_RUNTEST    _IOW(__JTAG_IOCTL_MAGIC, 7, unsigned int)
#else
#define JTAG_SIOCTRST   _IOW(__JTAG_IOCTL_MAGIC, 7, unsigned int)
#endif

const char *tap_state_name(tap_state_t state);
tap_state_t tap_state_by_name(const char *name);
STATUS JTAG_set_tap_state(JTAG_Handler *jtag, JtagStates tap_state);
STATUS JTAG_get_tap_state(JTAG_Handler *jtag);
STATUS JTAG_run_test(JTAG_Handler *jtag, JtagStates tap_state, unsigned int tcks);
STATUS JTAG_set_clock_frequency(JTAG_Handler *jtag, unsigned int frequency);
STATUS JTAG_set_mode(JTAG_Handler *jtag, unsigned int Mode);
int JTAG_set_jtag_trst(JTAG_Handler *jtag, unsigned int active);
int JTAG_ir_scan(JTAG_Handler *jtag, int num_bits, const uint8_t *out_bits, uint8_t *in_bits,
	tap_state_t state);
int JTAG_dr_scan(JTAG_Handler *jtag, int num_bits, const uint8_t *out_bits, uint8_t *in_bits,
	tap_state_t state);
int handle_svf_command(JTAG_Handler* jtag, char *filename);
void DBG_log(unsigned int level, const char *format, ...);

#endif
