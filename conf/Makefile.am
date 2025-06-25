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