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

orgdocdir = $(docdir)/org
htmldocdir = $(docdir)/html

orgdoc_DATA = \
	getting-started.org \
	common-config.org

nodist_htmldoc_DATA = \
	getting-started.html

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

if ENABLE_DOC
all-local: $(nodist_htmldoc_DATA)
CLEANFILES = $(nodist_htmldoc_DATA)
endif

.org.html:
	@echo "  EMACSGEN  $@" && $(EMACS_BATCH) $< --eval "(org-html-export-to-html)"
