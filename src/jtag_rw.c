/* Copyright (c) 2023, Nuvoton Corporation */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>
#include "../include/jtag.h"

#define XFER_CMD	(1 << 0)
#define XFER_DATA	(1 << 1)
#define DIR_R		(1 << 0)
#define DIR_W		(1 << 1)
struct jtagrw_xfer {
	char *cmd;
	char *data;
	int cmd_len;
	int data_len;
	int cmd_bitlen;
	int data_bitlen;
	int dir;
	int type;
};

static int args_to_xfer(struct jtagrw_xfer *xfer, char *arg, int type)
{
	char *data_ptrs[256];
	int len, i = 0;
	uint8_t *tmp;

	data_ptrs[i] = strtok(arg, ",");

	while (data_ptrs[i] && i < 255)
		data_ptrs[++i] = strtok(NULL, ",");

	tmp = (uint8_t *)calloc(i, sizeof(uint8_t));
	if (!tmp)
		return -1;

	for (len = 0; len < i; len++)
		tmp[len] = (uint8_t)strtol(data_ptrs[len], NULL, 0);

	if (type == XFER_CMD) {
		xfer->cmd_len = len;
		xfer->cmd = tmp;
	} else if (type == XFER_DATA) {
		xfer->data_len = len;
		xfer->data = tmp;
	}

	return 0;
}

void showUsage(char **argv)
{
	fprintf(stderr, "Usage: %s [option(s)]\n", argv[0]);
	fprintf(stderr, "  -d <intf>             jtag interface\n");
	fprintf(stderr, "                        (/dev/jtagX: jtag device)\n");
	fprintf(stderr, "                        (mctp: af_mctp socket)\n");
	fprintf(stderr, "  -e <eid>              target mctp eid if using mctp\n");
	fprintf(stderr, "  -n <net>              mctp net id if using mctp\n");
	fprintf(stderr, "  -c <command>          send 8-bit command\n");
	fprintf(stderr, "  -w <data>             write data\n");
	fprintf(stderr, "  -l <data bit length>  data bit length\n");
	fprintf(stderr, "  -t <tcks>             runtest idle\n");
	fprintf(stderr, "  -r                    print received data\n");
	fprintf(stderr, "  -i                    reset tap (TLR->RTI)\n");
}

int main(int argc, char **argv)
{
	char *jtag_dev = NULL;
	uint32_t instruction;
	uint32_t bit_len = 0;
	int c = 0, v, i;
	struct jtagrw_xfer xfer;
	int tcks = 0;
	bool reset = false;
	int ret;
	JTAG_Handler *handler;
	struct jtag_args args = {};

	memset(&xfer, 0, sizeof(xfer));
	xfer.cmd_bitlen = 8;
	while ((c = getopt(argc, argv, "d:e:n:c:w:l:t:ri")) != -1) {
		switch (c) {
		case 'd': {
			jtag_dev = malloc(strlen(optarg) + 1);
			strcpy(jtag_dev, optarg);
			break;
		}
		case 'e': {
			v = atoi(optarg);
			jtag_args_add(&args, ARG_EID, v & 0xff);
			break;
		}
		case 'n': {
			v = atoi(optarg);
			jtag_args_add(&args, ARG_NET, v & 0xff);
			break;
		}
		case 'c': {
			if (args_to_xfer(&xfer, optarg, XFER_CMD)) {
				exit(EXIT_FAILURE);
			}
			xfer.type |= XFER_CMD;
			break;
		}
		case 'w': {
			if (args_to_xfer(&xfer, optarg, XFER_DATA)) {
				exit(EXIT_FAILURE);
			}
			xfer.dir | DIR_W;
			xfer.type |= XFER_DATA;
			break;
		}
		case 'l': {
			bit_len = strtoul(optarg, NULL, 0);
			xfer.data_bitlen = bit_len;
			break;
		}
		case 't': {
			tcks = strtoul(optarg, NULL, 0);
			break;
		}
		case 'r': {
			xfer.dir |= DIR_R;
			xfer.type |= XFER_DATA;
			break;
		}
		case 'i': {
			reset = true;
			break;
		}
		default:  // h, ?, and other
			showUsage(argv);
			exit(EXIT_SUCCESS);
		}
	}
	if (optind < argc) {
		fprintf(stderr, "invalid non-option argument(s)\n");
		showUsage(argv);
		exit(EXIT_SUCCESS);
	}

	if (!jtag_dev) {
		showUsage(argv);
		goto exit;
	}

	if (xfer.data && (xfer.data_len * 8 < xfer.data_bitlen)) {
		printf("invalid data len\n");
		goto exit;
	}

	handler = JTAG_open(jtag_dev, &args);
	if (!handler) {
		fprintf(stderr, "Failed to open JTAG\n");
		goto exit;
	}

	if (reset)
		JTAG_reset_state(handler);

	//printf("xfer type = 0x%x\n", xfer.type);
	if (xfer.cmd && (xfer.type & XFER_CMD)) {
		ret = JTAG_send_command(handler, xfer.cmd, xfer.cmd_bitlen);
		if (ret) {
			printf("send command error\n");
			goto exit;
		}
	}
	if (xfer.type & XFER_DATA) {
		int i;
		int len = (xfer.data_bitlen + 7) / 8;
		if (!xfer.data) {
			xfer.data = malloc(len);
			if (!xfer.data)
				goto exit;
			memset(xfer.data, 0, len);
		}
		ret = JTAG_transfer_data(handler, xfer.data, xfer.data, xfer.data_bitlen);
		if (ret)
			goto exit;
		if (xfer.dir & DIR_R) {
			printf("Recv:\n");
			for (i = 0; i < len; i++)
				printf("%02x ", xfer.data[i]);
			printf("\n");
		}
	}
	if (tcks)
		JTAG_runtest_idle(handler, tcks);
exit:
	if (handler)
		JTAG_close(handler);
	if (jtag_dev)
		free(jtag_dev);
	if (xfer.data)
		free(xfer.data);
	if (xfer.cmd)
		free(xfer.cmd);

	return 0;
}
