EXTRA_DIST = freehttpd.service.in

if ENABLE_SYSTEMD
systemdunitdir = $(systemdsystemunitdir)
systemdunit_DATA = freehttpd.service

CLEANFILES = freehttpd.service

freehttpd.service: freehttpd.service.in
	@echo "  GEN      $@" && sed "s|\@bindir\@|$(bindir)|g" $< > $@
endif