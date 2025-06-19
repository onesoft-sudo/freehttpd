AM_CPPFLAGS = -I$(top_srcdir)/src
check_PROGRAMS = test_itable test_utils
TESTS = test_itable test_utils

test_itable_SOURCES = test_itable.c ../src/itable.c ../src/itable.h
test_utils_SOURCES = test_utils.c ../src/utils.c ../src/utils.h

check-valgrind-benchmark:
	$(SHELL) valgrind-benchmark.sh

check-benchmark:
	$(SHELL) benchmark.sh

.PHONY: check-valgrind-benchmark check-benchmark

clean-local:
	rm -f vgcore.* *.log
	rm -rf .deps