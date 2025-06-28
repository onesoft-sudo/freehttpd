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

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/core -I$(top_srcdir)/src/mm -I$(top_srcdir)/src/utils -I$(top_srcdir)/src/include
AM_TESTS_ENVIRONMENT = \
  top_srcdir=$(top_srcdir) \
  top_builddir=$(top_builddir) \
  testdir=$(top_builddir)/tests \
  VALGRIND=$(top_srcdir)/build-aux/valgrind

check_PROGRAMS = itable.test.helper utils.test.helper bitmap.test.helper base64.test.helper pool.test.helper
TESTS = itable.test utils.test bitmap.test base64.test pool.test

itable_test_helper_SOURCES = itable.test.c $(top_srcdir)/src/utils/itable.c $(top_srcdir)/src/utils/itable.h
utils_test_helper_SOURCES = utils.test.c $(top_srcdir)/src/utils/utils.c $(top_srcdir)/src/utils/utils.h
bitmap_test_helper_SOURCES = bitmap.test.c $(top_srcdir)/src/utils/bitmap.c $(top_srcdir)/src/utils/bitmap.h
base64_test_helper_SOURCES = base64.test.c $(top_srcdir)/src/utils/base64.c $(top_srcdir)/src/utils/base64.h
pool_test_helper_SOURCES = pool.test.c $(top_srcdir)/src/mm/pool.c $(top_srcdir)/src/mm/pool.h

EXTRA_DIST = $(TESTS)

check-valgrind-benchmark:
	$(SHELL) valgrind-benchmark.sh

check-benchmark:
	$(SHELL) benchmark.sh

.PHONY: check-valgrind-benchmark check-benchmark

clean-local:
	rm -f vgcore.* *.log
	rm -rf .deps
