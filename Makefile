export CC = gcc
export CFLAGS = -g -O2 -Wall -Wextra -pedantic -std=gnu23 -D_POSIX_C_SOURCE=200809L
export LDFLAGS = 
export RM = rm -f
export MKDIR = mkdir -p
export CP = cp -f

SUBDIRS = res src

export srcdir = "$(shell pwd)/src"

.PHONY: all clean check check-valgrind

all:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

	$(MKDIR) bin
	$(CP) $(srcdir)/freehttpd bin

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

	$(RM) -r bin

check: all
	@$(MAKE) -C tests

check-valgrind: all
	@$(MAKE) -C tests check-valgrind
