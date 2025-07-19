#                                               -*- Automake -*-
# Process this file with automake to produce Makefile.in.
#
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

noinst_LIBRARIES = libevent.a
libevent_a_SOURCES = \
	xpoll.c \
	xpoll.h \
	accept.c \
	accept.h \
	recv.c \
	recv.h \
	send.c \
	send.h

AM_CFLAGS = $(EXPORTED_AM_CFLAGS)
AM_CPPFLAGS = $(EXPORTED_AM_CPPFLAGS)
AM_LDFLAGS = $(EXPORTED_AM_LDFLAGS)

clean-local:
	rm -rf .deps