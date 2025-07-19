#                                               -*- Automake -*-
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

SUBDIRS = \
	res \
	conf \
	systemd \
	src \
	doc \
	man \
	tests

EXTRA_DIST = \
	README \
	NEWS \
	AUTHORS \
	TODO \
	bootstrap \
	ChangeLog \
	.tarball-version \
	build-aux/valgrind \
	build-aux/genchangelog \
	build-aux/update-license-comments

ACLOCAL_AMFLAGS = -I m4

GPG ?= @GPG@
GPG_LOCAL_USER ?=

vcs-clean: clean
	for dir in $(SUBDIRS) .; do \
		rm -rf \
			$$dir/autom4te.cache \
			$$dir/config.log \
			$$dir/config.status \
			$$dir/Makefile \
			$$dir/Makefile.in \
			$$dir/aclocal.m4 \
			$$dir/configure \
			$$dir/config.h \
			$$dir/config.h.in \
			$$dir/stamp-h1 \
			$$dir/*~ \
			$$dir/.deps \
			$$dir/.dirstamp; \
	done

check-benchmark: all
	$(MAKE) -C tests check-benchmark

check-valgrind-benchmark: all
	$(MAKE) -C tests check-valgrind-benchmark

.PHONY: vcs-clean check-benchmark check-valgrind-benchmark dist-fhttpd dist-fhttpd-sign

.tarball-version:
	echo "@VERSION@" > .tarball-version

distclean-local:
	rm -f .tarball-version

dist-hook:
	@echo "  UPDATE   ChangeLog" && "$(top_srcdir)/build-aux/genchangelog" -f plain -o "$(top_srcdir)/ChangeLog"

dist-fhttpd: dist dist-shar
dist-fhttpd-sign: dist-fhttpd
	if test -z "$(GPG_LOCAL_USER)"; then \
		echo "The GPG_LOCAL_USER variable is not set - please set it to your GPG key ID."; \
		exit 1; \
	fi

	@for format in tar.gz tar.xz tar.bz2 shar.gz; do \
		echo "  SIGN     $(distdir).$$format"; \
		$(GPG) --batch --yes -a -b --local-user $(GPG_LOCAL_USER) "$(distdir).$$format"; \
	done
