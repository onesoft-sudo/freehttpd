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
htmldocdir = $(docdir)/html

orgdoc_DATA = \
	index.org \
	licensing.org \
	gfdl.org \
	getting-started.org \
	command-line.org \
	managing-the-daemon.org \
	common-config.org

nodist_htmldoc_DATA = \
	index.html \
	licensing.html \
	gfdl.html \
	getting-started.html \
	command-line.html \
	managing-the-daemon.html

EXTRA_DIST = $(orgdoc_DATA)
SUFFIXES = .org .html

EMACS_LOAD_CONFIG = \
	--eval "(progn \
	(require 'package) \
	(package-initialize) \
	(require 'htmlize) \
	(message \"htmlize loaded\") \
	(setq-default image-scaling-factor 1) \
	(global-font-lock-mode t))"

EMACS_BATCH = $(EMACS) --batch $(EMACS_LOAD_CONFIG)

all-local: $(nodist_htmldoc_DATA)
CLEANFILES = $(nodist_htmldoc_DATA)

.org.html:
	@echo "  EMACSGEN  $@" && $(EMACS_BATCH) $< --eval "(org-html-export-to-html)" >> emacs.log 2>&1

endif
