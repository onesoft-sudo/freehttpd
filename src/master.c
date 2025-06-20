#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "log.h"
#include "server.h"

/* Only used by the worker processes */
static struct fhttpd_server *local_server = NULL;

static void
fhttpd_worker_exit_handler (void)
{
	if (local_server)
		fhttpd_server_destroy (local_server);
}

static void
fhttpd_worker_signal_handler (int signum)
{
	if (signum == SIGTERM)
	{
		fhttpd_wclog_info ("Received signal %s, shutting down worker process", strsignal (signum));
		exit (0);
	}
}

static _Noreturn void
fhttpd_worker_start (struct fhttpd_master *master)
{
	struct sigaction sa;

	sa.sa_handler = &fhttpd_worker_signal_handler;
	sa.sa_flags = 0;
	sigemptyset (&sa.sa_mask);

	if (sigaction (SIGTERM, &sa, NULL) < 0)
	{
		fhttpd_wclog_error ("Failed to set up signal handlers: %s", strerror (errno));
		exit (EXIT_FAILURE);
	}

	atexit (&fhttpd_worker_exit_handler);

	struct fhttpd_server *server = fhttpd_server_create (master);

	if (!server)
	{
		fhttpd_wclog_error ("Failed to create server: %s\n", strerror (errno));
		exit (EXIT_FAILURE);
	}

	local_server = server;

	if (!fhttpd_server_prepare (server))
	{
		fhttpd_wclog_error ("Failed to prepare server: %s\n", strerror (errno));
		exit (EXIT_FAILURE);
	}

	fhttpd_server_loop (server);
	fhttpd_server_destroy (server);

	local_server = NULL;
	exit (EXIT_FAILURE);
}

static void
fhttpd_print_info (const struct fhttpd_master *master)
{
	uint16_t *ports = (uint16_t *) master->config[FHTTPD_CONFIG_PORTS];

	while (*ports)
	{
		fhttpd_log_info ("Listening on port %d", *ports);
		ports++;
	}
}

bool
fhttpd_master_start (struct fhttpd_master *master)
{
	size_t worker_count = *(size_t *) master->config[FHTTPD_CONFIG_WORKER_PROCESS_COUNT];

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

	fhttpd_print_info (master);

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
