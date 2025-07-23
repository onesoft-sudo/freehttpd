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

noinst_LIBRARIES = libmodules.a
libmodules_a_SOURCES = \
	mod_autoindex.c \
	mod_autoindex.h

AM_CFLAGS = $(EXPORTED_AM_CFLAGS)
AM_CPPFLAGS = $(EXPORTED_AM_CPPFLAGS)
AM_LDFLAGS = $(EXPORTED_AM_LDFLAGS)

modulesdir = @FHTTPD_MODULE_PATH@
modules_LTLIBRARIES =
COMMON_MODULE_CFLAGS = -module -avoid-version -shared

if ENABLE_MOD_FOO
modules_LTLIBRARIES += mod_foo.la
mod_foo_la_SOURCES = mod_foo.c
mod_foo_la_LDFLAGS = $(COMMON_MODULE_CFLAGS)
endif

clean-local:
	rm -rf .deps
