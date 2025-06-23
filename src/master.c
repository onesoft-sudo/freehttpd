#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define FHTTPD_LOG_MODULE_NAME "master"

#include "compat.h"
#include "log.h"
#include "server.h"
#include "worker.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

bool
fhttpd_master_prepare (struct fhttpd_master *master)
{
#ifdef FHTTPD_MAIN_CONFIG_FILE
	const char *confpath = FHTTPD_MAIN_CONFIG_FILE;
#else
	const char *confpath = "/etc/freehttpd/fhttpd.conf";
#endif

	struct fhttpd_conf_parser *parser = fhttpd_conf_parser_create (confpath);

	if (!parser)
	{
		fhttpd_log_error ("Failed to parse config file: %s: %s", confpath, strerror (errno));
		return false;
	}

	int rc;

	if ((rc = fhttpd_conf_parser_read (parser)) < 0)
	{
		fhttpd_log_error ("Failed to read config file: %s: %s", confpath, strerror (errno));
		fhttpd_conf_parser_destroy (parser);
		return false;
	}

	struct fhttpd_config *config = fhttpd_conf_process (parser);

	if (!config)
	{
		enum conf_parser_error rc = fhttpd_conf_parser_last_error (parser);

		if (rc == CONF_PARSER_ERROR_SYNTAX_ERROR || rc == CONF_PARSER_ERROR_INVALID_CONFIG)
			fhttpd_conf_parser_print_error (parser);
		else
			fhttpd_log_error ("Failed to parse configuration file: %s\n", fhttpd_conf_parser_strerror (rc));

		fhttpd_conf_parser_destroy (parser);
		return false;
	}

	fhttpd_conf_print_config (config, 0);
	fhttpd_conf_parser_destroy (parser);

	master->config = config;
	return true;
}

bool
fhttpd_master_start (struct fhttpd_master *master)
{
	size_t worker_count = master->config->worker_count;

	master->workers = calloc (worker_count, sizeof (pid_t));

	if (!master->workers)
		return false;

	master->worker_count = worker_count;

	for (size_t i = 0; i < worker_count; i++)
	{
		pid_t pid = fork ();

		if (pid < 0)
			return false;

		if (pid == 0)
		{
			fhttpd_worker_start (master);
		}
		else
		{
			master->workers[i] = pid;
			fhttpd_log_info ("Started worker process: %d", pid);
		}
	}

	for (size_t i = 0; i < worker_count; i++)
		waitpid (master->workers[i], NULL, 0);

	return false;
}

void
fhttpd_master_destroy (struct fhttpd_master *master)
{
	if (!master)
		return;

	if (master->pid == getpid ())
	{
		for (size_t i = 0; i < master->worker_count; i++)
			kill (master->workers[i], SIGTERM);

		if (master->config)
			fhttpd_conf_free_config (master->config);
	}

	free (master->workers);
	free (master);
}

struct fhttpd_master *
fhttpd_master_create (void)
{
	struct fhttpd_master *master = calloc (1, sizeof (struct fhttpd_master));

	if (!master)
		return NULL;

	master->pid = getpid ();
	return master;
}
