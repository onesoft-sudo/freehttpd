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

freehttpd_sysconfdir = $(sysconfdir)/freehttpd
freehttpd_confdir = $(freehttpd_sysconfdir)/conf.d
freehttpd_hostdir = $(freehttpd_sysconfdir)/hosts.d

freehttpd_sysconf_DATA = fhttpd.conf

freehttpd_conf_DATA = conf.d/logging.conf conf.d/security.conf
freehttpd_host_DATA = hosts.d/localhost.conf

EXTRA_DIST = $(freehttpd_conf_DATA) $(freehttpd_host_DATA) $(freehttpd_sysconf_DATA)  fhttpd.conf.in

fhttpd.conf: fhttpd.conf.in ../config.status
	@echo "  GEN      $@" && sed "s|\@sysconfdir\@|$(sysconfdir)|g" $< > $@

CLEANFILES = fhttpd.conf