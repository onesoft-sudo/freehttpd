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

int
main (int argc __attribute_maybe_unused__, char **argv __attribute_maybe_unused__)
{
	fhttpd_log_set_output (stderr, stdout);

	struct fhttpd_master *master = fhttpd_master_create ();

	if (!master)
	{
		fprintf (stderr, "Failed to create server\n");
		return 1;
	}

	if (!fhttpd_master_prepare (master))
	{
		fhttpd_master_destroy (master);
		return 1;
	}

	if (!fhttpd_master_start (master))
	{
		fprintf (stderr, "Failed to run server: %s\n", strerror (errno));
		fhttpd_master_destroy (master);
		return 1;
	}

	fhttpd_master_destroy (master);
	return 0;
}
