/*
 * This file is part of OSN freehttpd.
 * 
 * Copyright (C) 2025  OSN Developers.
 *
 * OSN freehttpd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * OSN freehttpd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with OSN freehttpd.  If not, see <https://www.gnu.org/licenses/>.
 */

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
	local_server->flag_terminate = true;
}

static void
fhttpd_worker_sigquit_handler (int _signum __attribute_maybe_unused__)
{
	fhttpd_wclog_info ("Received SIGQUIT, reloading configuration");
	local_server->flag_clean_quit = true;
}

static void
fhttpd_worker_setup_signals (void)
{
	struct sigaction sa = { 0 };

	sa.sa_handler = &fhttpd_worker_sigterm_handler;
	sa.sa_flags = SA_RESTART;
	sa.sa_flags = 0;

	if (sigaction (SIGTERM, &sa, NULL) < 0)
	{
		fhttpd_wclog_error ("Failed to set up signal handlers: %s", strerror (errno));
		exit (EXIT_FAILURE);
	}

	memset (&sa, 0, sizeof sa);
	sa.sa_handler = &fhttpd_worker_sigquit_handler;
	sa.sa_flags = SA_RESTART;

	if (sigaction (SIGQUIT, &sa, NULL) < 0)
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
