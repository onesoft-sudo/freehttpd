#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define FH_LOG_MODULE_NAME "master"

#include "conf.h"
#include "log/log.h"
#include "master.h"
#include "worker.h"

#define FH_MASTER_SPAWN_WORKERS 4

static struct fh_master *local_master = NULL;
static bool should_exit = false;

struct fh_master *
fh_master_create (void)
{
	return calloc (1, sizeof (struct fh_master));
}

void
fh_master_destroy (struct fh_master *master)
{
	if (master->worker_pids)
	{
		for (size_t i = 0; i < master->worker_count; i++)
		{
			kill (master->worker_pids[i], SIGTERM);
			fh_pr_info ("Sent SIGTERM to worker: %d", master->worker_pids[i]);
		}

		for (size_t i = 0; i < master->worker_count; i++)
			waitpid (master->worker_pids[i], NULL, 0);

		free (master->worker_pids);
	}

	if (master->config)
    	fhttpd_conf_free_config (master->config);
	
	free (master);
}

static void
fh_master_handle_term (int sig)
{
	fh_pr_info ("Received %s", sig == SIGTERM ? "SIGTERM" : "SIGINT");
	should_exit = true;
}

bool
fh_master_setup_signal (struct fh_master *master)
{
	local_master = master;

	struct sigaction act;

	act.sa_flags = SA_RESTART;
	act.sa_handler = &fh_master_handle_term;
	sigemptyset (&act.sa_mask);

	if (sigaction (SIGINT, &act, NULL) < 0 || sigaction (SIGTERM, &act, NULL) < 0)
		return false;

	act.sa_flags = SA_RESTART;
	act.sa_handler = SIG_IGN;
	sigemptyset (&act.sa_mask);

	return sigaction (SIGHUP, &act, NULL) == 0;
}

static bool
fh_master_reset_signal (void)
{
	struct sigaction act;

	act.sa_flags = 0;
	act.sa_handler = SIG_DFL;
	sigemptyset (&act.sa_mask);

	if (sigaction (SIGINT, &act, NULL) < 0 || sigaction (SIGTERM, &act, NULL) < 0)
		return false;

	return sigaction (SIGINT, &act, NULL) == 0 && sigaction (SIGTERM, &act, NULL) == 0
		   && sigaction (SIGHUP, &act, NULL) == 0;
}

bool
fh_master_read_config (struct fh_master *master)
{
#ifndef FHTTPD_MAIN_CONFIG_FILE
	#define FHTTPD_MAIN_CONFIG_FILE "/etc/freehttpd/fhttpd.conf"
#endif

	const char *config_file = FHTTPD_MAIN_CONFIG_FILE;

	struct fhttpd_conf_parser *parser = fhttpd_conf_parser_create (config_file);

	if (!parser)
	{
		fh_pr_err ("Failed to read configuration file: %s", strerror (errno));
		return false;
	}

	if (fhttpd_conf_parser_read (parser) < 0)
	{
		fh_pr_err ("Failed to read configuration file: %s", strerror (errno));
		fhttpd_conf_parser_destroy (parser);
		return false;
	}

	struct fhttpd_config *config = fhttpd_conf_process (parser);

	if (!config)
	{
		fh_pr_err ("Failed to parse configuration file:");
		fhttpd_conf_parser_print_error (parser);
		fhttpd_conf_parser_destroy (parser);
		return false;
	}

	master->config = config;
	fhttpd_conf_parser_destroy (parser);

	fh_pr_info ("Read configuration file successfully");
	fhttpd_conf_print_config (config, 0);

	return true;
}

bool
fh_master_spawn_workers (struct fh_master *master)
{
	master->worker_pids = calloc (FH_MASTER_SPAWN_WORKERS, sizeof (pid_t));

	if (!master->worker_pids)
		return false;

	master->worker_count = FH_MASTER_SPAWN_WORKERS;

	for (size_t i = 0; i < FH_MASTER_SPAWN_WORKERS; i++)
	{
		pid_t pid = fork ();

		if (pid < 0)
			return false;

		if (pid == 0)
		{
			local_master = NULL;
			fh_master_reset_signal ();
			struct fhttpd_config *config = master->config;
			free (master->worker_pids);
			free (master);
			fh_worker_start (config);
		}
		else
		{
			master->worker_pids[i] = pid;
			fh_pr_info ("Started worker process with PID %d", pid);
		}
	}

	fh_pr_info ("Master PID: %d", getpid ());
	return true;
}

void
fh_master_wait (struct fh_master *master)
{
	for (size_t i = 0; i < master->worker_count; i++)
	{
		if (should_exit)
		{
			if (local_master)
				fh_master_destroy (local_master);

			exit (EXIT_SUCCESS);
		}

		waitpid (master->worker_pids[i], NULL, 0);
	}
}
