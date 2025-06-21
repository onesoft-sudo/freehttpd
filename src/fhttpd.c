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

#if 0
	struct fhttpd_conf_parser *conf_parser = fhttpd_conf_parser_create ("conf/fhttpd.conf");
	int rc;

	if (!conf_parser)
	{
		fprintf (stderr, "%s: Failed to create configuration parser: %s\n", argv[0], strerror (errno));
		return 1;
	}

	if ((rc = fhttpd_conf_parser_read (conf_parser)) != 0)
	{
		fprintf (stderr, "%s: Failed to read configuration file: %s\n", argv[0], strerror (rc));
		fhttpd_conf_parser_destroy (conf_parser);
		return 1;
	}

	struct fhttpd_config *config = fhttpd_conf_process (conf_parser);

	if (!config)
	{
		auto rc = fhttpd_conf_parser_last_error (conf_parser);

		if (rc == CONF_PARSER_ERROR_SYNTAX_ERROR || rc == CONF_PARSER_ERROR_INVALID_CONFIG)
			fhttpd_conf_parser_print_error (conf_parser);
		else
			fprintf (stderr, "%s: Failed to parse configuration file: %s\n", argv[0], fhttpd_conf_parser_strerror (rc));
	}
	else
	{
		fhttpd_conf_print_config (config, 0);
		fhttpd_conf_free_config (config);
	}

	fhttpd_conf_parser_destroy (conf_parser);
	exit (0);
#endif

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
