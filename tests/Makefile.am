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