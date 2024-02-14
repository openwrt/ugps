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

#ifndef __TTY_H_
#define __TTY_H_

#include <termios.h>

#include <libubox/ustream.h>

extern char longitude[33], latitude[33], course[17], speed[17], elevation[17], satellites[3], hdop[5];
extern int nmea_open(char *dev, struct ustream_fd *s, speed_t speed);
extern void gps_timestamp(void);
extern unsigned int adjust_clock;
extern char gps_fields;
#define GPS_FIELD_LAT (1<<0)
#define GPS_FIELD_LON (1<<1)
#define GPS_FIELD_COG (1<<2)
#define GPS_FIELD_SPD (1<<3)
#define GPS_FIELD_ALT (1<<4)
#define GPS_FIELD_SAT (1<<5)
#define GPS_FIELD_HDP (1<<6)

#endif
