#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FHTTPD_LOG_MODULE_NAME "main"

#include "conf.h"
#include "log.h"
#include "server.h"
#include "strutils.h"

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef FHTTPD_ENABLE_SYSTEMD
	#include <systemd/sd-daemon.h>
#endif /* FHTTPD_ENABLE_SYSTEMD */

int
main (int argc __attribute_maybe_unused__, char **argv __attribute_maybe_unused__)
{
	fhttpd_log_set_output (stderr, stdout);

	struct fhttpd_master *master = fhttpd_master_create ();

	if (!master)
	{
		sd_notifyf (0, "ERRNO=%d", errno);
		fhttpd_log_error ("Failed to create server");
		return 1;
	}

	if (!fhttpd_master_prepare (master))
	{
		sd_notifyf (0, "ERRNO=%d", errno);
		fhttpd_master_destroy (master);
		return 1;
	}

	if (!fhttpd_master_start (master))
	{
		sd_notifyf (0, "ERRNO=%d", errno);
		fhttpd_log_error ("Failed to run server: %s", strerror (errno));
		fhttpd_master_destroy (master);
		return 1;
	}

	fhttpd_master_destroy (master);
	return 0;
}
