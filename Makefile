export CC = gcc
export CFLAGS = -g -O0 -Wall -Wextra -pedantic -std=gnu11 -D_POSIX_C_SOURCE=200809L
export LDFLAGS = 
export RM = rm -f
export MKDIR = mkdir -p
export CP = cp -f

SUBDIRS = src

export srcdir = "$(shell pwd)/src"

.PHONY: all clean tests

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

tests:
	@$(MAKE) -C tests