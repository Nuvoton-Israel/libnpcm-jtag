#ifndef __JTAG_API_H__
#define __JTAG_API_H__

#define MAX_FREQ	50
#define LOG_LEVEL_ERR	3
#define LOG_LEVEL_INFO	2
#define LOG_LEVEL_DEBUG	1

#define JTAG_MODE_HW	0
#define JTAG_MODE_SW	1

/* frequency: 0 (use default) */
int JTAG_open(char *jtag_dev, int frequency, int mode);
void JTAG_close(int handle);
void JTAG_reset_state(int handle);
int JTAG_load_svf(int handle, char *svf_path, int single_step);
void JTAG_set_loglevel(int handle, unsigned int level);
int JTAG_send_command(int handle, uint8_t *command, uint32_t bit_len);
int JTAG_transfer_data(int handle, uint8_t *out, uint8_t *in, uint32_t bit_len);
void JTAG_runtest_idle(int handle, uint32_t tcks);

#endif
