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

SUBDIRS = core log utils include event hash mm http digest router modules

bin_PROGRAMS = freehttpd

freehttpd_SOURCES = \
	freehttpd.c

freehttpd_LDADD = \
	$(top_builddir)/src/core/libcore.a \
	$(top_builddir)/src/event/libevent.a \
	$(top_builddir)/src/core/libcore.a \
	$(top_builddir)/src/log/liblog.a \
	$(top_builddir)/src/hash/libhash.a \
	$(top_builddir)/src/mm/libmm.a \
	$(top_builddir)/src/router/librouter.a \
	$(top_builddir)/src/http/libhttp.a \
	$(top_builddir)/src/digest/libdigest.a \
	$(top_builddir)/src/modules/libmodules.a \
	$(top_builddir)/src/utils/libutils.a \
	$(top_builddir)/res/libresources.a \
	$(SYSTEMD_LIBS)

FHTTPD_MAIN_CONFIG_FILE = @FHTTPD_MAIN_CONFIG_FILE@

export EXPORTED_AM_CFLAGS = 
export EXPORTED_AM_LDFLAGS = 
export EXPORTED_AM_CPPFLAGS = -I$(ABS_SRCDIR)/src/include -I$(ABS_SRCDIR)/src -I$(ABS_SRCDIR)/res -DHAVE_RESOURCES '-DFHTTPD_MAIN_CONFIG_FILE="$(FHTTPD_MAIN_CONFIG_FILE)"'

AM_CFLAGS = $(EXPORTED_AM_CFLAGS)
AM_CPPFLAGS = $(EXPORTED_AM_CPPFLAGS)
AM_LDFLAGS = $(EXPORTED_AM_LDFLAGS)

clean-local:
	rm -rf .deps
