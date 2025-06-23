#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define FHTTPD_LOG_MODULE_NAME "worker"

#include "compat.h"
#include "log.h"
#include "master.h"
#include "server.h"
#include "worker.h"

static struct fhttpd_server *local_server = NULL;
static struct fhttpd_master *local_master = NULL;

static void
fhttpd_worker_exit_handler (void)
{
	if (local_server)
		fhttpd_server_destroy (local_server);

	if (local_master)
		fhttpd_master_destroy (local_master);
}

static void
fhttpd_worker_sigterm_handler (int _signum __attribute_maybe_unused__)
{
	fhttpd_wclog_info ("Received SIGTERM, shutting down worker process");
	exit (0);
}

static void
fhttpd_worker_sighup_handler (int _signum __attribute_maybe_unused__)
{
	fhttpd_wclog_info ("Received SIGHUP, reading opcode");

	fd_t in_fd = local_server->pipe_fd[0];
	enum fhttpd_ipc_op op = 0;

	if (read (in_fd, &op, 1) != 1)
	{
		fhttpd_wclog_error ("Failed to read IPC opcode: %s", strerror (errno));
		return;
	}

	switch (op)
	{
		case FHTTPD_IPC_RELOAD_CONFIG:
			if (!fhttpd_master_reload_config (local_master))
				fhttpd_wclog_error ("Unable to reload configuration");

			local_server->config = local_master->config;
			fhttpd_server_config_host_map (local_server);
			break;

		default:
			fhttpd_wclog_info ("Unsupported opcode: %02x", op);
			break;
	}
}

static void
fhttpd_worker_setup_signals (void)
{
	struct sigaction sa = { 0 };

	sa.sa_handler = &fhttpd_worker_sigterm_handler;
	sa.sa_flags = 0;

	if (sigaction (SIGTERM, &sa, NULL) < 0)
	{
		fhttpd_wclog_error ("Failed to set up signal handlers: %s", strerror (errno));
		exit (EXIT_FAILURE);
	}

	memset (&sa, 0, sizeof sa);
	sa.sa_handler = &fhttpd_worker_sighup_handler;
	sa.sa_flags = SA_RESTART;

	if (sigaction (SIGHUP, &sa, NULL) < 0)
	{
		fhttpd_wclog_error ("Failed to set up signal handlers: %s", strerror (errno));
		exit (EXIT_FAILURE);
	}

	memset (&sa, 0, sizeof sa);
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = SA_RESTART;

	if (sigaction (SIGINT, &sa, NULL) < 0)
	{
		fhttpd_wclog_error ("Failed to set up signal handlers: %s", strerror (errno));
		exit (EXIT_FAILURE);
	}
}

__noreturn void
fhttpd_worker_start (struct fhttpd_master *master, fd_t pipe_fd[static 2])
{
	fhttpd_worker_setup_signals ();
	atexit (&fhttpd_worker_exit_handler);

	local_master = master;

	struct fhttpd_server *server = fhttpd_server_create (master, master->config, pipe_fd);

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
