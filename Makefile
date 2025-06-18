export CC = gcc
export DBG_FLAGS = -march=native -fstack-protector-strong -fstack-protector-strong \
	-fno-omit-frame-pointer
export CFLAGS = -g -O2 -Wall -Wextra -pedantic -std=gnu23 -D_POSIX_C_SOURCE=200809L
export LDFLAGS = -g

ifeq ($(DEBUG), 1)
	export CFLAGS += $(DBG_FLAGS)
	export LDFLAGS += $(DBG_FLAGS)
else
	export CFLAGS += -DNDEBUG
endif

ifeq ($(ANALYZE), 1)
	export CFLAGS += -fanalyzer
	export LDFLAGS += -fanalyzer
endif

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
