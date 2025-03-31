/* Copyright (c) 2025, Nuvoton Corporation */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>

#include "../include/jtag.h"
#include "../include/mctp.h"

#define MCTP_MESSAGE_TYPE_OEM_JTAG 0x5F

#define CMD_JTAG_SET_STATE      1
#define CMD_JTAG_TRANSFER       2
struct jtag_xfer2 {
	uint8_t type;
	uint8_t direction;
	uint8_t from;
	uint8_t endstate;
	uint32_t padding;
	uint32_t length;
	uint8_t	tdio[];
}__attribute__((packed));

struct jtag_tap_state2 {
	uint8_t	reset;
	uint8_t	from;
	uint8_t	endstate;
	uint32_t tck;
}__attribute__((packed));

struct mctp_jtag_msg {
	uint8_t cmd;
	uint8_t data[];
}__attribute__((packed));

struct jtag_mctp_priv {
	int frequency;
	int loglevel;
	int eid;
	int net;
};

static struct jtag_mctp_priv jtag_priv = {
	.frequency = 0,
	.loglevel = LEV_INFO,
	.eid = 0,
	.net = 1,
};

static int poll_file(int fd, int timeout)
{
	struct pollfd fds[1];
	int rc;

	fds[0].fd = fd;
	fds[0].events = POLLIN | POLLERR;

	rc = poll(fds, 1, timeout);

	if (rc > 0) {
		//printf("poll events=0x%x\n", fds[0].revents);
		if (fds[0].revents == POLLERR) {
			printf("poll error\n");
			return -1;
		}
		return fds[0].revents;
	}

	if (rc < 0) {
		printf("Poll returned error status (errno=%d)\n", errno);

		return -1;
	} else if (rc == 0) {
		//printf("Poll timeout\n");
		return -1;
	}

	return 0;
}
static int mctp_send(int sd, int net, int eid, uint8_t *data, int len)
{
	struct sockaddr_mctp_ext addr;
	socklen_t addrlen;
	int rc, val;
	size_t i;

	if (eid == 0) {
		printf("invalid eid\n");
		return -1;
	}
	memset(&addr, 0x0, sizeof(addr));
	addrlen = sizeof(struct sockaddr_mctp);
	addr.smctp_base.smctp_family = AF_MCTP;
	addr.smctp_base.smctp_network = net;
	addr.smctp_base.smctp_addr.s_addr = eid;
	addr.smctp_base.smctp_type = MCTP_MESSAGE_TYPE_OEM_JTAG;
	addr.smctp_base.smctp_tag = MCTP_TAG_OWNER;

	rc = sendto(sd, data, len, 0, (struct sockaddr *)&addr, addrlen);
	if (rc != len) {
		perror("sendto");
		return -1;
	}


	return 0;
}

static int mctp_recv(int sd, int net, int eid, uint8_t *data, int len)
{
	struct sockaddr_mctp_ext addr;
	socklen_t addrlen;
	int rc;

	if (eid == 0) {
		printf("invalid eid\n");
		return -1;
	}
	if (poll_file(sd, 3000) < 0) {
		perror("poll error");
		return -1;
	}

	addrlen = sizeof(addr);
	rc = recvfrom(sd, data, len, MSG_TRUNC,
			(struct sockaddr *)&addr, &addrlen);
	if (rc < 0) {
		perror("recv error");
		return -1;
	}

	return 0;
}

static void jtag_mctp_process_args(JTAG_Handler *handler, struct jtag_args *args)
{
	int i;

	for (i = 0; i < args->num_args; i++) {
		if (i >= JTAG_MAX_ARGS)
			return;
		if (args->arg[i].id == ARG_FREQ)
			jtag_priv.frequency = args->arg[i].val;
		else if (args->arg[i].id == ARG_LOG_LEVEL)
			jtag_priv.loglevel = args->arg[i].val;
		else if (args->arg[i].id == ARG_EID)
			jtag_priv.eid = args->arg[i].val;
		else if (args->arg[i].id == ARG_NET)
			jtag_priv.net = args->arg[i].val;
	}
}

static int jtag_mctp_open(JTAG_Handler *handler, char *jtag_dev, struct jtag_args *args)
{
	int sd;

	sd = socket(AF_MCTP, SOCK_DGRAM, 0);
	if (sd < 0) {
		perror("Can't open AF_MCTP socket");
		return -1;
	}

	jtag_mctp_process_args(handler, args);
	handler->handle = sd;
	handler->loglevel = jtag_priv.loglevel;

	return 0;
}

static void jtag_mctp_close(JTAG_Handler *handler)
{
	close(handler->handle);
}

int jtag_mctp_run_tck(JTAG_Handler *handler, int tap_state, int tcks)
{
	struct mctp_jtag_msg *req;
	struct jtag_tap_state2 *set_state;
	uint8_t *buf;
	int msg_len = sizeof(struct mctp_jtag_msg) + sizeof(struct jtag_tap_state2);
	int net = jtag_priv.net;
	int eid = jtag_priv.eid;
	int rc;

	buf = malloc(msg_len);
	if (!buf)
		return -1;
	req = (struct mctp_jtag_msg *)buf;
	set_state = (struct jtag_tap_state2 *)&req->data[0];

	req->cmd = CMD_JTAG_SET_STATE;
	set_state->reset = 0;
	set_state->from = JTAG_STATE_CURRENT;
	set_state->endstate = tap_state;
	set_state->tck = tcks;
	/* send request */
	rc = mctp_send(handler->handle, net, eid, buf, msg_len);
	if (rc < 0)
		goto err_ret;
	/* recv response */
	rc = mctp_recv(handler->handle, net, eid, buf, sizeof(struct mctp_jtag_msg));
	if (rc < 0)
		goto err_ret;

	handler->tap_state = tap_state;
err_ret:
	free(buf);
	return rc;

}

static int jtag_mctp_set_tap_state(JTAG_Handler *handler, int tap_state)
{
	return jtag_mctp_run_tck(handler, tap_state, 0);
}

static int jtag_mctp_shift(JTAG_Handler *handler, int type, int bits,
		const uint8_t *out, uint8_t *in, int state)
{
	struct mctp_jtag_msg *req;
	struct jtag_xfer2 *xfer;
	int data_bytes = (bits + 7) / 8;
	int msg_len = sizeof(struct mctp_jtag_msg) + sizeof(struct jtag_xfer2) + data_bytes;
	uint8_t *buf;
	int net = jtag_priv.net;
	int eid = jtag_priv.eid;
	int rc;

	buf = malloc(msg_len);
	if (!buf)
		return -1;
	req = (struct mctp_jtag_msg *)buf;
	xfer = (struct jtag_xfer2 *)&req->data[0];
	req->cmd = CMD_JTAG_TRANSFER;
	xfer->type = type;
	xfer->direction = 0;
	xfer->from = JTAG_STATE_CURRENT;
	xfer->endstate = state;
	xfer->padding = 0;
	xfer->length = bits;
	memcpy(xfer->tdio, out, data_bytes);
	/* send request */
	rc = mctp_send(handler->handle, net, eid, buf, msg_len);
	if (rc < 0)
		return rc;
	/* recv response */
	rc = mctp_recv(handler->handle, net, eid, buf, sizeof(struct mctp_jtag_msg) + data_bytes);
	if (rc < 0)
		return rc;

	if (in)
		memcpy(in, buf + sizeof(struct mctp_jtag_msg), data_bytes);
err_ret:
	free(buf);
	return rc;
}

static int jtag_mctp_shift_dr(JTAG_Handler *handler, int bits, const uint8_t *out, uint8_t *in, int state)
{
	return jtag_mctp_shift(handler, JTAG_SDR_XFER, bits, out, in, state);
}

static int jtag_mctp_shift_ir(JTAG_Handler *handler, int bits, const uint8_t *out, uint8_t *in, int state)
{
	return jtag_mctp_shift(handler, JTAG_SIR_XFER, bits, out, in, state);
}

static const struct jtag_ops jtag_mctp_ops = {
	.open = jtag_mctp_open,
	.close = jtag_mctp_close,
	.set_state = jtag_mctp_set_tap_state,
	.run_tck = jtag_mctp_run_tck,
	.shift_dr = jtag_mctp_shift_dr,
	.shift_ir = jtag_mctp_shift_ir,
};

JTAG_Handler jtag_mctp_handler = {
	.name = "jtag_mctp",
	.type = JTAG_INTF_MCTP,
	.priv = &jtag_priv,
	.ops = &jtag_mctp_ops,
};
