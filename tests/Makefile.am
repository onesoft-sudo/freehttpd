#           -*- mode: makefile-automake; -*-
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

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/src/core -I$(top_srcdir)/src/mm -I$(top_srcdir)/src/utils -I$(top_srcdir)/src/include
AM_TESTS_ENVIRONMENT = \
  top_srcdir=$(top_srcdir) \
  top_builddir=$(top_builddir) \
  testdir=$(top_builddir)/tests \
  VALGRIND=$(top_srcdir)/build-aux/valgrind

check_PROGRAMS = itable.test.helper path.test.helper base64.test.helper pool.test.helper strtable.test.helper
TESTS = itable.test path.test base64.test pool.test strtable.test

itable_test_helper_SOURCES = itable.test.c $(top_srcdir)/src/hash/itable.c $(top_srcdir)/src/hash/itable.h
strtable_test_helper_SOURCES = strtable.test.c $(top_srcdir)/src/hash/strtable.c $(top_srcdir)/src/hash/strtable.h
path_test_helper_SOURCES = path.test.c $(top_srcdir)/src/utils/path.c $(top_srcdir)/src/utils/path.h
base64_test_helper_SOURCES = base64.test.c $(top_srcdir)/src/digest/base64.c $(top_srcdir)/src/digest/base64.h
pool_test_helper_SOURCES = pool.test.c $(top_srcdir)/src/mm/pool.c $(top_srcdir)/src/mm/pool.h

EXTRA_DIST = $(TESTS)

check-valgrind-benchmark:
	BINDIR=$(top_srcdir)/src $(SHELL) valgrind-benchmark.sh

check-benchmark:
	BINDIR=$(top_srcdir)/src $(SHELL) benchmark.sh

.PHONY: check-valgrind-benchmark check-benchmark

clean-local:
	rm -f vgcore.* *.log
	rm -rf .deps
