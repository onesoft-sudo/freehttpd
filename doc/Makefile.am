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

info_TEXINFOS = manual.texi
html_FILES = manual.html
info_FILES = manual.info
AM_DISTCHECK_DVI_TARGET = html

EXTRA_DIST = $(info_TEXINFOS) getting-started.org
AM_MAKEINFOHTMLFLAGS = --no-split
CLEANFILES = $(html_FILES) $(info_FILES)

all-local: $(html_FILES)

# This is a way to trick Automake into not showing warnings
# regarding the overrides of the existing `pdf', `dvi' and `ps'
# rules.  It is hacky, but it works.
