#                                               -*- mode: makefile-automake; -*-
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

if ENABLE_DOC
orgdocdir = $(docdir)/org

orgdoc_DATA = \
	index.org \
	common-config.org \
	introduction/licensing.org \
	introduction/gfdl.org \
	introduction/getting-started.org \
	invoking/command-line.org \
	invoking/managing-the-daemon.org

nodist_doc_DATA = \
	html/index.html \
	html/introduction/licensing.html \
	html/introduction/gfdl.html \
	html/introduction/getting-started.html \
	html/invoking/command-line.html \
	html/invoking/managing-the-daemon.html

EXTRA_DIST = $(orgdoc_DATA)
CLEANFILES = $(nodist_doc_DATA)
OUTPUT_DIR = html

all-local: $(OUTPUT_DIR)

clean-local:
	rm -rf $(OUTPUT_DIR)

html: $(nodist_doc_DATA)

$(nodist_doc_DATA): $(orgdoc_DATA)
	@echo "  EMACSGENHTML $(OUTPUT_DIR)" && $(top_srcdir)/build-aux/org-doc-gen -E "$(EMACS)" -o "$(OUTPUT_DIR)" .

endif
