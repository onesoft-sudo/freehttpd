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

# SUBDIRS = 

bin_PROGRAMS = freehttpd

freehttpd_SOURCES = \
	freehttpd.c

freehttpd_LDADD = \
	$(top_builddir)/res/libresources.a \
	$(SYSTEMD_LIBS)

FHTTPD_MAIN_CONFIG_FILE = @FHTTPD_MAIN_CONFIG_FILE@

export EXPORTED_AM_CFLAGS = -Wall -Wextra -pedantic -std=gnu99 $(SYSTEMD_CFLAGS)
export EXPORTED_AM_CPPFLAGS = -I$(ABS_SRCDIR)/src/include -I$(ABS_SRCDIR)/src -I$(ABS_SRCDIR)/res -DHAVE_RESOURCES '-DFHTTPD_MAIN_CONFIG_FILE="$(FHTTPD_MAIN_CONFIG_FILE)"'

AM_CFLAGS = $(EXPORTED_AM_CFLAGS)
AM_CPPFLAGS = $(EXPORTED_AM_CPPFLAGS)

clean-local:
	rm -rf .deps
