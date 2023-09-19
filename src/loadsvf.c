/* Copyright (c) 2023, Nuvoton Corporation */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>
#include "../include/jtag_api.h"

void showUsage(char **argv)
{
	fprintf(stderr, "Usage: %s [option(s)]\n", argv[0]);
	fprintf(stderr, "  -d <device>   jtag device\n");
	fprintf(stderr, "  -l <level>    log level\n");
	fprintf(stderr, "  -m <mode>     transfer mode\n");
	fprintf(stderr, "                (1: PSPI mode)\n");
	fprintf(stderr, "                (0: GPIO mode)\n");
	fprintf(stderr, "  -f <freq>     force running at frequency (Mhz)\n");
	fprintf(stderr, "                for PSPI mode\n");
	fprintf(stderr, "  -s <filepath> load svf file\n");
	fprintf(stderr, "  -g            run svf command line by line\n\n");
}

int main(int argc, char **argv)
{
	char *svf_path = NULL;
	char *jtag_dev = NULL;
	int c = 0;
	int v;
	bool single_step = false;
	int frequency = 0;
	int mode = JTAG_MODE_HW;
	int loglevel = LOG_LEVEL_INFO;
	int handle = -1;

	while ((c = getopt(argc, argv, "m:f:l:s:d:i:r:g")) != -1) {
		switch (c) {
		case 'l': {
			loglevel = atoi(optarg);
			break;
		}
		case 'm': {
			v = atoi(optarg);
			if (v >= 0 && v <= 1) {
				if (v == 1)
					mode = JTAG_MODE_HW;
				else if (v == 0)
					mode = JTAG_MODE_SW;
			}
			break;
		}
		case 'f': {
			v = atoi(optarg);
			if (v > 0 && v <= MAX_FREQ) {
				frequency = v * 1000000;
			}
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

	handle = JTAG_open(jtag_dev, frequency, mode);
	if (handle == -1) {
		fprintf(stderr, "Failed to open JTAG\n");
		goto exit;
	}
	JTAG_set_loglevel(handle, loglevel);
	JTAG_load_svf(handle, svf_path, single_step);
	JTAG_close(handle);
exit:
	if (svf_path)
		free(svf_path);
	if (jtag_dev)
		free(jtag_dev);

	return 0;
}
