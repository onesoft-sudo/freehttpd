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

EXTRA_DIST = freehttpd.service.in

if ENABLE_SYSTEMD
systemdunitdir = $(systemdsystemunitdir)
systemdunit_DATA = freehttpd.service

CLEANFILES = freehttpd.service

freehttpd.service: freehttpd.service.in
	@echo "  GEN      $@" && sed "s|\@bindir\@|$(bindir)|g" $< > $@
endif