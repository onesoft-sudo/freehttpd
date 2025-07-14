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

noinst_LIBRARIES = libhttp.a
libhttp_a_SOURCES = \
	http1_request.c \
	http1_request.h \
	http1_response.c \
	http1_response.h \
	http1.h \
	protocol.c \
	protocol.h

AM_CFLAGS = $(EXPORTED_AM_CFLAGS)
AM_CPPFLAGS = $(EXPORTED_AM_CPPFLAGS)

clean-local:
	rm -rf .deps
