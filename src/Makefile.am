# This file is part of OSN freehttpd.
#
# Copyright (C) 2025  OSN Developers.
#
# OSN freehttpd is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# OSN freehttpd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
# 
# You should have received a copy of the GNU Affero General Public License
# along with OSN freehttpd.  If not, see <https://www.gnu.org/licenses/>.

bin_PROGRAMS = freehttpd
FHTTPD_MAIN_CONFIG_FILE = @FHTTPD_MAIN_CONFIG_FILE@

AM_CFLAGS = -Wall -Wextra -pedantic -std=gnu99 $(SYSTEMD_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/res -DHAVE_RESOURCES '-DFHTTPD_MAIN_CONFIG_FILE="$(FHTTPD_MAIN_CONFIG_FILE)"'

freehttpd_SOURCES = \
	fhttpd.c \
	autoindex.c \
	http1.c \
	connection.c \
	itable.c \
	itable.h \
	log.c \
	protocol.c \
	server.c \
	utils.c \
	conf.c \
	autoindex.h \
	http1.h \
	connection.h \
	log.h \
	protocol.h \
	server.h \
	utils.h \
	types.h \
	conf.h \
	loop.h \
	master.c \
	master.h \
	strutils.c \
	strutils.h \
	strtable.c \
	strtable.h \
	bitmap.c \
	bitmap.h \
	compat.h \
	worker.c \
	worker.h \
	base64.c \
	base64.h

freehttpd_LDADD = ../res/libresources.a $(SYSTEMD_LIBS)

clean-local:
	rm -rf .deps