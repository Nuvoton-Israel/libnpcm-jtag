/* Copyright (c) 2023, Nuvoton Corporation */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include "../include/jtag.h"

void showUsage(char **argv)
{
	fprintf(stderr, "Usage: %s [option(s)]\n", argv[0]);
	fprintf(stderr, "  -d <intf>     jtag interface\n");
	fprintf(stderr, "                (/dev/jtagX: jtag device)\n");
	fprintf(stderr, "                (mctp: af_mctp socket)\n");
	fprintf(stderr, "  -m <mode>     jtag mode if using jtag device\n");
	fprintf(stderr, "                (0: HW mode)\n");
	fprintf(stderr, "                (1: SW mode)\n");
	fprintf(stderr, "  -e <eid>      target mctp eid if using mctp\n");
	fprintf(stderr, "  -n <net>      mctp net id if using mctp\n");
	fprintf(stderr, "  -l <level>    log level\n");
	fprintf(stderr, "  -f <freq>     force running at frequency (Mhz)\n");
	fprintf(stderr, "                for jtag device(HW mode)\n");
	fprintf(stderr, "  -s <filepath> svf file path\n");
	fprintf(stderr, "  -g            run svf command line by line\n\n");
}

int main(int argc, char **argv)
{
	char *svf_path = NULL;
	char *jtag_dev = NULL;
	int c = 0;
	int v, i;
	bool single_step = false;
	int frequency = 0;
	int priv = JTAG_MODE_HW;
	struct timeval start, end;
	unsigned long diff;
	JTAG_Handler *handler;
	struct jtag_args args = {};

	while ((c = getopt(argc, argv, "d:m:e:n:l:f:s:g")) != -1) {
		switch (c) {
		case 'l': {
			v = atoi(optarg);
			if (v >= 0 && v < 3)
				jtag_args_add(&args, ARG_LOG_LEVEL, v);
			break;
		}
		case 'm': {
			v = atoi(optarg);
			if (v == 0 || v == 1)
				jtag_args_add(&args, ARG_MODE, v);
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
		case 'f': {
			v = atoi(optarg);
			if (v > 0 && v <= MAX_FREQ) {
				frequency = v * 1000000;
			}
			jtag_args_add(&args, ARG_FREQ, frequency);
			break;
		}
		case 'g': {
			single_step = true;
			break;
		}
		case 'd': {
			jtag_dev = malloc(strlen(optarg) + 1);
			strcpy(jtag_dev, optarg);
			break;
		}
		case 's': {
			svf_path = malloc(strlen(optarg) + 1);
			strcpy(svf_path, optarg);
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

	if (!svf_path || !jtag_dev) {
		showUsage(argv);
		goto exit;
	}

	handler = JTAG_open(jtag_dev, &args);
	if (!handler) {
		fprintf(stderr, "Failed to open JTAG\n");
		goto exit;
	}
	JTAG_reset_state(handler);

	gettimeofday(&start,NULL);
	JTAG_load_svf(handler, svf_path, single_step);
	gettimeofday(&end,NULL);
	diff = 1000 * (end.tv_sec-start.tv_sec)+ (end.tv_usec-start.tv_usec) / 1000;
	printf("Programming time is %ld ms\n",diff);
	//printf("JTAG TCK freq=%d\n", JTAG_get_clock_frequency(handler));
	//printf("total runtest time is %ld ms\n", total_runtest_time / 1000);

	JTAG_close(handler);
exit:
	if (svf_path)
		free(svf_path);
	if (jtag_dev)
		free(jtag_dev);

	return 0;
}
