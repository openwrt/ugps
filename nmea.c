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

#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE
#define _BSD_SOURCE
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#include <string.h>
#include <termios.h>

#include <libubox/utils.h>

#include "log.h"
#include "nmea.h"

#define MAX_NMEA_PARAM	20
#define MAX_TIME_OFFSET	5
#define MAX_BAD_TIME	3

struct nmea_param {
	char *str;
	int num;
} nmea_params[MAX_NMEA_PARAM];

static int nmea_bad_time;
char longitude[33] = { 0 }, latitude[33] = { 0 }, course[17] = { 0 }, speed[17] = { 0 }, elevation[17] = { 0 }, satellites[3] = { 0 }, hdop[5] = { 0 };
int gps_valid = 0;
char gps_fields = 0;

static void
nmea_txt_cb(void)
{
	char *ids[] = { "ERROR", "WARNING", "NOTICE", };

	if (nmea_params[3].num < 0 || nmea_params[3].num > 2)
		nmea_params[3].num = 0;

	DEBUG(3, "%s: %s\n", ids[nmea_params[3].num], nmea_params[4].str);
}

static void
do_adjust_clock(struct tm *tm)
{
	char tmp[256];

	strftime(tmp, 256, "%Y-%m-%dT%H:%M:%S", tm);
	DEBUG(3, "date: %s UTC\n", tmp);

	if (adjust_clock) {
		time_t sec = timegm(tm);
		struct timeval cur;

		gettimeofday(&cur, NULL);

		if ((sec < 0) || (llabs(cur.tv_sec - sec) > MAX_TIME_OFFSET)) {
			struct timeval tv = { 0 };
			tv.tv_sec = sec;
			if (++nmea_bad_time > MAX_BAD_TIME) {
				LOG("system time differs from GPS time by more than %d seconds. Using %s UTC as the new time\n", MAX_TIME_OFFSET, tmp);
				/* only set datetime if specified by command line argument! */
				settimeofday(&tv, NULL);
			}
		} else {
			nmea_bad_time = 0;
		}
	}
}

static void
parse_gps_coords(char *latstr, char *vhem, char *lonstr, char *hhem)
{
	float minutes;
	float degrees;
	float lat = strtof(latstr, NULL);
	float lon = strtof(lonstr, NULL);

	degrees = floor(lat / 100.0);
	minutes = lat - (degrees * 100.0);
	lat = degrees + minutes / 60.0;

	degrees = floor(lon / 100.0);
	minutes = lon - (degrees * 100.0);
	lon = degrees + minutes / 60.0;

	if (*vhem == 'S')
		lat *= -1.0;
	if (*hhem == 'W')
		lon *= -1.0;

	snprintf(latitude, sizeof(latitude), "%f", lat);
	snprintf(longitude, sizeof(longitude), "%f", lon);

	DEBUG(3, "position: %s %s\n", latitude, longitude);
	gps_fields |= GPS_FIELD_LAT | GPS_FIELD_LON;

	gps_timestamp();
}

static void
nmea_rmc_cb(void)
{
	struct tm tm;

	if (*nmea_params[2].str != 'A') {
		gps_valid = 0;
		DEBUG(4, "waiting for valid signal\n");
		return;
	}

	gps_valid = 1;
	memset(&tm, 0, sizeof(tm));
	tm.tm_isdst = 1;

	if (sscanf(nmea_params[1].str, "%02d%02d%02d",
		&tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 3) {
		ERROR("failed to parse time '%s'\n", nmea_params[1].str);
	}
	else if (sscanf(nmea_params[9].str, "%02d%02d%02d",
		&tm.tm_mday, &tm.tm_mon, &tm.tm_year) != 3) {
		ERROR("failed to parse date '%s'\n", nmea_params[9].str);
	}
	else if (tm.tm_year == 0) {
		DEBUG(4, "waiting for valid date\n");
		return;
	}
	else {
		tm.tm_year += 100; /* year starts with 1900 */
		tm.tm_mon -= 1; /* month starts with 0 */

		do_adjust_clock(&tm);
	}

	if (strlen(nmea_params[3].str) < 9 || strlen(nmea_params[5].str) < 10) {
		ERROR("lat/lng have invalid string length %zu<9, %zu<10\n",
		       strlen(nmea_params[3].str), strlen(nmea_params[5].str));
	} else {
		parse_gps_coords(nmea_params[3].str, nmea_params[4].str, nmea_params[5].str, nmea_params[6].str);
	}
}

static void
nmea_zda_cb(void)
{
	struct tm tm;

	if (!gps_valid)
		return;

	memset(&tm, 0, sizeof(tm));
	tm.tm_isdst = 1;

	if (sscanf(nmea_params[1].str, "%02d%02d%02d",
		&tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 3) {
		ERROR("failed to parse time '%s'\n", nmea_params[1].str);
		return;
	}

	if ((sscanf(nmea_params[2].str, "%02d", &tm.tm_mday) != 1) ||
	    (sscanf(nmea_params[3].str, "%02d", &tm.tm_mon) != 1) ||
	    (sscanf(nmea_params[4].str, "%04d", &tm.tm_year) != 1)) {
		ERROR("failed to parse time '%s,%s,%s'\n",
			nmea_params[2].str, nmea_params[3].str, nmea_params[4].str);
		return;
	}

	if (tm.tm_year == 0) {
		DEBUG(4, "waiting for valid date\n");
		return;
	}

	tm.tm_mon -= 1; /* month starts with 0 */
	tm.tm_year -= 1900; /* full 4-digit year, tm expects years till 1900 */

	do_adjust_clock(&tm);
}

static void
nmea_gll_cb(void)
{
	if (*nmea_params[6].str != 'A') {
		gps_valid = 0;
		DEBUG(4, "waiting for valid signal\n");
		return;
	}

	gps_valid = 1;

	parse_gps_coords(nmea_params[1].str, nmea_params[2].str, nmea_params[3].str, nmea_params[4].str);
}

static void
nmea_gga_cb(void)
{
	if (!gps_valid)
		return;
	strncpy(satellites, nmea_params[7].str, sizeof(satellites));
	strncpy(hdop, nmea_params[8].str, sizeof(hdop));
	strncpy(elevation, nmea_params[9].str, sizeof(elevation));
	gps_fields |= GPS_FIELD_SAT | GPS_FIELD_HDP | GPS_FIELD_ALT;
	DEBUG(4, "satellites: %s\n", satellites);
	DEBUG(4, "HDOP: %s\n", hdop);
	DEBUG(4, "height: %s\n", elevation);
}

static void
nmea_vtg_cb(void)
{
	if (!gps_valid)
		return;
	strncpy(course, nmea_params[1].str, sizeof(course));
	strncpy(speed, nmea_params[7].str, sizeof(speed));
	gps_fields |= GPS_FIELD_COG | GPS_FIELD_SPD;
	DEBUG(4, "course: %s\n", course);
	DEBUG(4, "speed: %s\n", speed);
}

static struct nmea_msg {
	char *msg;
	int cnt;
	void (*handler) (void);
} nmea_msgs[] = {
	{
		.msg = "TXT",
		.cnt = 5,
		.handler = nmea_txt_cb,
	}, {
		.msg = "RMC",
		.cnt = 11,
		.handler = nmea_rmc_cb,
	}, {
		.msg = "GGA",
		.cnt = 14,
		.handler = nmea_gga_cb,
	}, {
		.msg = "GLL",
		.cnt = 7,
		.handler = nmea_gll_cb,
	}, {
		.msg = "VTG",
		.cnt = 9,
		.handler = nmea_vtg_cb,
	}, {
		.msg = "ZDA",
		.cnt = 5,
		.handler = nmea_zda_cb,
	},
};

static int
nmea_verify_checksum(char *s)
{
        char *csum = strrchr(s, '*');
	int isum, c = 0;

	if (!csum)
		return -1;

	*csum = '\0';
	csum++;
	isum = strtol(csum, NULL, 16);

	while(*s)
		c ^= *s++;

	if (isum != c)
		return -1;

	return 0;
}

static int
nmea_tokenize(char *msg)
{
	int cnt = 0;
	char *tok = strsep(&msg, ",");

	while (tok && cnt < MAX_NMEA_PARAM) {
		nmea_params[cnt].str = tok;
		nmea_params[cnt].num = atoi(tok);
		cnt++;
		tok = strsep(&msg, ",");
	}

	return cnt;
}

static void
nmea_process(char *a)
{
	char *csum;
	int cnt;
	unsigned int i;

	if (strncmp(a, "$GP", 3) &&
	    strncmp(a, "$GN", 3))
		return;

	a++;
	csum = strrchr(a, '*');
	if (!csum)
		return;

	if (nmea_verify_checksum(a)) {
		ERROR("nmea message has invalid checksum\n");
		return;
	}

	cnt = nmea_tokenize(&a[2]);
	if (cnt < 0) {
		ERROR("failed to tokenize %s\n", a);\
		return;
	}

	for (i = 0; i < ARRAY_SIZE(nmea_msgs); i++) {
		if (strcmp(nmea_params[0].str, nmea_msgs[i].msg) &&
		    strcmp(nmea_params[3].str, nmea_msgs[i].msg))
			continue;
		if (nmea_msgs[i].cnt <= cnt)
			nmea_msgs[i].handler();
		else
			ERROR("%s datagram has wrong parameter count got %d but expected %d\n", nmea_msgs[i].msg, cnt, nmea_msgs[i].cnt);
		return;
	}
}

static int
nmea_consume(struct ustream *s, char **a)
{
	char *eol = strstr(*a, "\n");

	if (!eol)
		return -1;

	*eol++ = '\0';

	nmea_process(*a);

	ustream_consume(s, eol - *a);
	*a = eol;

	return 0;
}

static void
nmea_msg_cb(struct ustream *s, int bytes)
{
	int len;
	char *a = ustream_get_read_buf(s, &len);

	while (!nmea_consume(s, &a))
		;
}

static void nmea_notify_cb(struct ustream *s)
{
	if (!s->eof)
		return;

	ERROR("tty error, shutting down\n");
	exit(-1);
}

int
nmea_open(char *dev, struct ustream_fd *s, speed_t speed)
{
	struct termios tio;
	int tty;

	tty = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (tty < 0) {
		ERROR("%s: device open failed: %s\n", dev, strerror(errno));
		return -1;
	}

	tcgetattr(tty, &tio);
	tio.c_cflag |= CREAD;
	tio.c_cflag |= CS8;
	tio.c_iflag |= IGNPAR;
	tio.c_lflag &= ~(ICANON);
	tio.c_lflag &= ~(ECHO);
	tio.c_lflag &= ~(ECHOE);
	tio.c_lflag &= ~(ISIG);
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	cfsetispeed(&tio, speed);
	cfsetospeed(&tio, speed);
	tcsetattr(tty, TCSANOW, &tio);

	s->stream.string_data = true;
	s->stream.notify_read = nmea_msg_cb;
	s->stream.notify_state = nmea_notify_cb;

	ustream_fd_init(s, tty);

	tcflush(tty, TCIFLUSH);

	return 0;
}
