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

AM_CPPFLAGS = -I$(top_srcdir)/src
AM_TESTS_ENVIRONMENT = \
  top_srcdir=$(top_srcdir) \
  top_builddir=$(top_builddir) \
  VALGRIND=$(top_srcdir)/build-aux/valgrind

check_PROGRAMS = test_itable_bin test_utils_bin test_bitmap_bin test_base64_bin
TESTS = test_itable test_utils test_bitmap test_base64

test_itable_bin_SOURCES = test_itable.c $(top_srcdir)/src/itable.c $(top_srcdir)/src/itable.h
test_utils_bin_SOURCES = test_utils.c $(top_srcdir)/src/utils.c $(top_srcdir)/src/utils.h
test_bitmap_bin_SOURCES = test_bitmap.c $(top_srcdir)/src/bitmap.c $(top_srcdir)/src/bitmap.h
test_base64_bin_SOURCES = test_base64.c $(top_srcdir)/src/base64.c $(top_srcdir)/src/base64.h

EXTRA_DIST = $(TESTS)

check-valgrind-benchmark:
	$(SHELL) valgrind-benchmark.sh

check-benchmark:
	$(SHELL) benchmark.sh

.PHONY: check-valgrind-benchmark check-benchmark

clean-local:
	rm -f vgcore.* *.log
	rm -rf .deps