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

LOG_COMPILER = $(top_srcdir)/build-aux/valgrind
EXTRA_DIST = $(top_srcdir)/build-aux/valgrind

AM_CPPFLAGS = -I$(top_srcdir)/src
check_PROGRAMS = test_itable test_utils test_bitmap
TESTS = test_itable test_utils test_bitmap

test_itable_SOURCES = test_itable.c ../src/itable.c ../src/itable.h
test_utils_SOURCES = test_utils.c ../src/utils.c ../src/utils.h
test_bitmap_SOURCES = test_bitmap.c ../src/bitmap.c ../src/bitmap.h

check-valgrind-benchmark:
	$(SHELL) valgrind-benchmark.sh

check-benchmark:
	$(SHELL) benchmark.sh

.PHONY: check-valgrind-benchmark check-benchmark

clean-local:
	rm -f vgcore.* *.log
	rm -rf .deps