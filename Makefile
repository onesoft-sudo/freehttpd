export CC = gcc
export CFLAGS = -g -Wall -Wextra -pedantic -std=gnu99 -D_POSIX_C_SOURCE=200809L
export LDFLAGS = 
export RM = rm -f

SUBDIRS = src

export srcdir = "$(shell pwd)/src"

.PHONY: all clean

all:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
