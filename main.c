/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 *   Copyright (C) 2014 John Crispin <blogic@openwrt.org> 
 */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <libubox/uloop.h>
#include <libubus.h>

#include "log.h"
#include "nmea.h"

unsigned int debug;
static struct ustream_fd stream;
static struct ubus_auto_conn conn;
static struct blob_buf b;
static char *ubus_socket;
struct timespec stamp = { 0 };
unsigned int adjust_clock = 0;

void
gps_timestamp(void)
{
	clock_gettime(CLOCK_MONOTONIC, &stamp);
}

static int
gps_info(struct ubus_context *ctx, struct ubus_object *obj,
	struct ubus_request_data *req, const char *method,
	struct blob_attr *msg)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);

	blob_buf_init(&b, 0);

	if (!stamp.tv_sec) {
		blobmsg_add_u8(&b, "signal", 0);
	} else {
		blobmsg_add_u32(&b, "age", now.tv_sec - stamp.tv_sec);
		blobmsg_add_string(&b, "latitude", latitude);
		blobmsg_add_string(&b, "longitude", longitude);
		blobmsg_add_string(&b, "elevation", elevation);
		blobmsg_add_string(&b, "course", course);
		blobmsg_add_string(&b, "speed", speed);
	}
	ubus_send_reply(ctx, req, b.head);

	return UBUS_STATUS_OK;
}

static const struct ubus_method gps_methods[] = {
	UBUS_METHOD_NOARG("info", gps_info),
};

static struct ubus_object_type gps_object_type =
	UBUS_OBJECT_TYPE("gps", gps_methods);

static struct ubus_object gps_object = {
	.name = "gps",
	.type = &gps_object_type,
	.methods = gps_methods,
	.n_methods = ARRAY_SIZE(gps_methods),
};

static void
ubus_connect_handler(struct ubus_context *ctx)
{
	int ret;

	ret = ubus_add_object(ctx, &gps_object);
	if (ret)
		fprintf(stderr, "Failed to add object: %s\n", ubus_strerror(ret));
}

static int
usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [options] <device>\n"
		"Options:\n"
		"	-a		Adjust system clock from gps\n"
		"	-s <path>	Path to ubus socket\n"
		"	-d <level>	Enable debug messages\n"
		"	-S		Print messages to stdout\n"
		"\n", prog);
	return -1;
}

int
main(int argc, char ** argv)
{
	int ch;
	char *device = NULL;
	char *dbglvl = getenv("DBGLVL");
	int ulog_channels = ULOG_KMSG;

	signal(SIGPIPE, SIG_IGN);

	if (dbglvl) {
		debug = atoi(dbglvl);
		unsetenv("DBGLVL");
	}

	while ((ch = getopt(argc, argv, "ad:s:S")) != -1) {
		switch (ch) {
		case 'a':
			adjust_clock = -1;
			break;
		case 's':
			ubus_socket = optarg;
			break;
		case 'd':
			debug = atoi(optarg);
			break;
		case 'S':
			ulog_channels = ULOG_STDIO;
			break;
		default:
			return usage(argv[0]);
		}
	}

	if (argc - optind < 1) {
		fprintf(stderr, "ERROR: missing device parameter\n");
		return usage(argv[0]);
	}

	device = argv[optind];
	ulog_open(ulog_channels, LOG_DAEMON, "ugps");

	uloop_init();
	conn.path = ubus_socket;
	conn.cb = ubus_connect_handler;
	ubus_auto_connect(&conn);

	if (nmea_open(device, &stream, B4800) < 0)
		return -1;

	uloop_run();
	uloop_done();

	return 0;
}
