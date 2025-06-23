#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

	fhttpd_master_destroy (fhttpd);
	fhttpd = NULL;
}

void
signal_handler (int signum)
{
	if (!fhttpd)
		exit (0);

	fprintf (stderr, "%s, shutting down %s...\n", strsignal (signum), fhttpd->pid == getpid () ? "server" : "worker");

	if (fhttpd->pid != getpid ())
	{
		struct sigaction sa;
		sa.sa_handler = SIG_DFL;
		sigemptyset (&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction (signum, &sa, NULL);
	}

	exit (0);
}

int
main (int argc __attribute_maybe_unused__, char **argv __attribute_maybe_unused__)
{
	atexit (&exit_handler);
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
		fprintf (stderr, "Failed to set up signal handlers\n");
		return 1;
	}

	if (!fhttpd_master_prepare (fhttpd))
		return 1;

	if (!fhttpd_master_start (fhttpd))
	{
		fprintf (stderr, "Failed to run server: %s\n", strerror (errno));
		return 1;
	}

	return 0;
}
