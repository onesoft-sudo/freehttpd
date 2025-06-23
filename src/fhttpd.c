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

static struct fhttpd_master *fhttpd = NULL;

void
exit_handler (void)
{
	if (!fhttpd)
		return;

	if (fhttpd->pid == getpid ())
		fhttpd_master_destroy (fhttpd);

	fhttpd = NULL;
}

void
signal_handler (int signum)
{
	if (!fhttpd)
		exit (0);

	if (fhttpd->pid == getpid ())
		{
			fprintf (stdout, "\n");
			fhttpd_log_info ("%s, shutting down master process", strsignal (signum));
		}

	exit_handler ();
	exit (0);
}

int
main (int argc __attribute_maybe_unused__, char **argv __attribute_maybe_unused__)
{
	fhttpd_log_set_output (stderr, stdout);

	fhttpd = fhttpd_master_create ();

	if (!fhttpd)
	{
		fprintf (stderr, "Failed to create server\n");
		return 1;
	}

	struct sigaction sa;

	sa.sa_handler = &signal_handler;
	sa.sa_flags = 0;
	sigemptyset (&sa.sa_mask);

	if (sigaction (SIGINT, &sa, NULL) < 0 || sigaction (SIGTERM, &sa, NULL) < 0)
	{
		exit_handler ();
		fprintf (stderr, "Failed to set up signal handlers\n");
		return 1;
	}

	if (!fhttpd_master_prepare (fhttpd))
	{
		exit_handler ();
		return 1;
	}

	if (!fhttpd_master_start (fhttpd))
	{
		fprintf (stderr, "Failed to run server: %s\n", strerror (errno));
		exit_handler ();
		return 1;
	}

	exit_handler ();
	return 0;
}
