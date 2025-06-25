bin_PROGRAMS = freehttpd
FHTTPD_MAIN_CONFIG_FILE = @FHTTPD_MAIN_CONFIG_FILE@

AM_CFLAGS = -Wall -Wextra -pedantic -std=gnu99 $(SYSTEMD_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/res -DHAVE_RESOURCES '-DFHTTPD_MAIN_CONFIG_FILE="$(FHTTPD_MAIN_CONFIG_FILE)"'

freehttpd_SOURCES = \
	fhttpd.c \
	autoindex.c \
	http1.c \
	connection.c \
	itable.c \
	log.c \
	protocol.c \
	server.c \
	utils.c \
	conf.c \
	autoindex.h \
	http1.h \
	connection.h \
	itable.h \
	log.h \
	protocol.h \
	server.h \
	utils.h \
	types.h \
	conf.h \
	loop.h \
	master.c \
	master.h \
	strutils.c \
	strutils.h \
	strtable.c \
	strtable.h \
	bitmap.c \
	bitmap.h \
	compat.h \
	worker.c \
	worker.h

freehttpd_LDADD = ../res/libresources.a $(SYSTEMD_LIBS)

clean-local:
	rm -rf .deps