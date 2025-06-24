#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define FHTTPD_LOG_MODULE_NAME "master"

#include "compat.h"
#include "log.h"
#include "server.h"
#include "utils.h"
#include "worker.h"

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

static struct fhttpd_master *local_master = NULL;

static bool flag_terminate = false;
static bool flag_reload_config = false;

bool
fhttpd_master_load_config (struct fhttpd_master *master)
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
fhttpd_master_reload_config (struct fhttpd_master *master)
{
	struct fhttpd_config *prev_config = master->config;

	if (!fhttpd_master_load_config (master))
		return false;

	fhttpd_conf_free_config (prev_config);
	return true;
}

static void
fhttpd_master_term_handler (int _signal __attribute_maybe_unused__)
{
	fhttpd_log_info ("SIGTERM received, terminating");
	flag_terminate = true;
}

static void
fhttpd_master_hup_handler (int _signal __attribute_maybe_unused__)
{
	fhttpd_log_info ("SIGHUP received, reloading configuration");
	flag_reload_config = true;
}

bool
fhttpd_master_prepare (struct fhttpd_master *master)
{
	struct sigaction sa = { 0 };

	sa.sa_flags = SA_RESTART;
	sa.sa_handler = &fhttpd_master_term_handler;

	if (sigaction (SIGTERM, &sa, NULL) < 0 || sigaction (SIGINT, &sa, NULL) < 0)
		return false;

	memset (&sa, 0, sizeof sa);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = &fhttpd_master_hup_handler;

	if (sigaction (SIGHUP, &sa, NULL) < 0)
		return false;

	return fhttpd_master_load_config (master);
}

bool
fhttpd_master_spawn_workers (struct fhttpd_master *master)
{
	size_t worker_count = master->config->worker_count;

	master->workers = calloc (worker_count, sizeof (pid_t));
	master->worker_pipes = calloc (worker_count, sizeof (fd_t[2]));

	if (!master->workers || !master->worker_pipes)
		return false;

	master->worker_count = worker_count;
	fhttpd_log_info ("Spawning %zu workers", master->worker_count);

	for (size_t i = 0; i < master->worker_count; i++)
	{
		fd_t pipe_fd[2];

		if (pipe (pipe_fd) < 0)
			return false;

		pid_t pid = fork ();

		if (pid < 0)
			return false;

		if (pid == 0)
		{
			fhttpd_worker_start (master, pipe_fd);
			_exit (1);
		}
		else
		{
			fd_set_nonblocking (pipe_fd[0]);
			fd_set_nonblocking (pipe_fd[1]);
			master->workers[i] = pid;
			memcpy (master->worker_pipes[i], pipe_fd, sizeof pipe_fd);
			fhttpd_log_info ("Started worker process: %d", pid);
		}
	}

	return true;
}

bool
fhttpd_master_start (struct fhttpd_master *master)
{
	if (!fhttpd_master_spawn_workers (master))
		return false;

	while (true)
	{
		pause ();

		if (flag_terminate)
		{
			if (master)
				fhttpd_master_destroy (master);

			exit (0);
		}

		if (flag_reload_config)
		{
			if (!fhttpd_master_reload_config (master))
			{
				flag_reload_config = false;
				continue;
			}

			for (size_t i = 0; i < master->worker_count; i++)
			{
				pid_t worker_pid = master->workers[i];
				fd_t *pipefd = master->worker_pipes[i];
				close (pipefd[0]);
				close (pipefd[1]);
				kill (worker_pid, SIGQUIT);
			}

			fhttpd_log_info ("Sent SIGQUIT to workers");

			for (size_t i = 0; i < master->worker_count; i++)
			{
				pid_t worker_pid = master->workers[i];
				waitpid (worker_pid, NULL, 0);
				fhttpd_log_info ("Worker %d terminated", worker_pid);
			}
			
			free (master->worker_pipes);
			free (master->workers);

			master->worker_count = 0;
			master->worker_pipes = NULL;
			master->workers = NULL;
			
			if (!fhttpd_master_spawn_workers (master))
				return false;

			flag_reload_config = false;
		}
	}

	for (size_t i = 0; i < master->config->worker_count; i++)
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

	free (master->worker_pipes);
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
	local_master = master;

	return master;
}
