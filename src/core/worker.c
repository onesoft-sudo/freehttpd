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

#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#define FH_LOG_MODULE_NAME "worker"

#include "log/log.h"
#include "worker.h"
#include "server.h"

static pid_t pid;
static struct fh_server *server = NULL;

static void
fh_worker_cleanup (void)
{
    if (server)
        fh_server_destroy (server);
}

static void
fh_worker_handle_signal (int sig __attribute_maybe_unused__)
{
    if (server)
        server->should_exit = true;
}

static bool
fh_worker_setup_signal (void)
{
	struct sigaction act;

	act.sa_flags = SA_RESTART;
	act.sa_handler = &fh_worker_handle_signal;
	sigemptyset (&act.sa_mask);

	if (sigaction (SIGINT, &act, NULL) < 0 || sigaction (SIGTERM, &act, NULL) < 0)
		return false;

	act.sa_flags = SA_RESTART;
	act.sa_handler = SIG_IGN;
	sigemptyset (&act.sa_mask);

	return sigaction (SIGHUP, &act, NULL) == 0;
}

_noreturn void 
fh_worker_start (struct fhttpd_config *config)
{
    pid = getpid ();

    if (pid <= 0)
        exit (EXIT_FAILURE);

    fh_log_set_worker_pid (pid);

    if (!fh_worker_setup_signal ())
        exit (EXIT_FAILURE);

    atexit (&fh_worker_cleanup);

    server = fh_server_create (config);

    if (!server)
    {
        fh_pr_emerg ("Failed to create server: %s", strerror (errno));
        exit (EXIT_FAILURE);
    }

    if (!fh_server_listen (server))
    {
        fh_pr_emerg ("Failed to initialize server: %s", strerror (errno));
        exit (EXIT_FAILURE);
    }

    fh_server_loop (server);
    exit (EXIT_SUCCESS);
}
