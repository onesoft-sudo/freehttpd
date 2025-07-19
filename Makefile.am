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

SUBDIRS = res conf systemd src doc man tests
EXTRA_DIST = README NEWS AUTHORS TODO bootstrap ChangeLog .tarball-version build-aux/valgrind
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
	echo '1.0.0' > .tarball-version

clean-local:
	rm -f .tarball-version

dist-fhttpd:
	$(MAKE) dist-gzip
	$(MAKE) dist-xz
	$(MAKE) dist-bzip2

dist-fhttpd-sign: dist-fhttpd
	if test -z "$(GPG_LOCAL_USER)"; then \
		echo "The GPG_LOCAL_USER variable is not set - please set it to your GPG key ID."; \
		exit 1; \
	fi

	$(GPG) -a -b --local-user $(GPG_LOCAL_USER) "$(distdir).tar.gz"
	$(GPG) -a -b --local-user $(GPG_LOCAL_USER) "$(distdir).tar.bz2"
	$(GPG) -a -b --local-user $(GPG_LOCAL_USER) "$(distdir).tar.xz"
