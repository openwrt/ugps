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

static struct ustream_fd stream;
static struct ubus_auto_conn conn;
static struct blob_buf b;
struct timespec stamp = { 0 };

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
		blobmsg_add_string(&b, "lattitude", lattitude);
		blobmsg_add_string(&b, "longitude", longitude);
		blobmsg_add_string(&b, "elivation", elivation);
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
usage(void)
{
	LOG("ugps <device>\n");
	return -1;
}

int
main(int argc, char ** argv)
{

	signal(SIGPIPE, SIG_IGN);

	if (argc != 2)
		return usage();

	uloop_init();
	conn.cb = ubus_connect_handler;
	ubus_auto_connect(&conn);
	nmea_open(argv[1], &stream, B4800);
	uloop_run();
	uloop_done();

	return 0;
}
