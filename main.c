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

	if (!stamp.tv_sec || !gps_fields) {
		blobmsg_add_u8(&b, "signal", 0);
	} else {
		blobmsg_add_u32(&b, "age", now.tv_sec - stamp.tv_sec);
		if (gps_fields & GPS_FIELD_LAT)
			blobmsg_add_string(&b, "latitude", latitude);
		if (gps_fields & GPS_FIELD_LON)
			blobmsg_add_string(&b, "longitude", longitude);
		if (gps_fields & GPS_FIELD_ALT)
			blobmsg_add_string(&b, "elevation", elevation);
		if (gps_fields & GPS_FIELD_COG)
			blobmsg_add_string(&b, "course", course);
		if (gps_fields & GPS_FIELD_SPD)
			blobmsg_add_string(&b, "speed", speed);
		if (gps_fields & GPS_FIELD_SAT)
			blobmsg_add_string(&b, "satellites", satellites);
		if (gps_fields & GPS_FIELD_HDP)
			blobmsg_add_string(&b, "HDOP", hdop);
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
		"	-b		Set gps device baud rate\n"
		"\n", prog);
	return -1;
}

static speed_t get_baudrate(int baudrate)
{
    switch (baudrate) {
		case 4800:
			return B4800;
		case 9600:
			return B9600;
		case 19200:
			return B19200;
		case 38400:
			return B38400;
		case 57600:
			return B57600;
		case 115200:
			return B115200;
		default:
			fprintf(stderr, "ERROR: incorrect baud rate. Default 4800 baud rate has been set\n");
			return B4800;
    }
}

int
main(int argc, char ** argv)
{
	int ch;
	char *device = NULL;
	char *dbglvl = getenv("DBGLVL");
	int ulog_channels = ULOG_KMSG;
	speed_t baudrate = B4800;

	signal(SIGPIPE, SIG_IGN);

	if (dbglvl) {
		debug = atoi(dbglvl);
		unsetenv("DBGLVL");
	}

	while ((ch = getopt(argc, argv, "ad:s:Sb:")) != -1) {
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
		case 'b':
			baudrate = get_baudrate(atoi(optarg));
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

	if (nmea_open(device, &stream, baudrate) < 0)
		return -1;

	uloop_run();
	uloop_done();

	return 0;
}
