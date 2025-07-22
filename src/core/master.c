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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define FH_LOG_MODULE_NAME "master"

#include "conf.h"
#include "confproc.h"
#include "log/log.h"
#include "master.h"
#include "modules/module.h"
#include "worker.h"

#ifdef HAVE_CONFPATHS_H
	#include "confpaths.h"
#endif /* HAVE_CONFPATHS_H */

#define FH_MASTER_SPAWN_WORKERS 8

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
		fh_conf_free (master->config);

	if (master->module_manager)
		fh_module_manager_free (master->module_manager);
	
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

	if (sigaction (SIGINT, &act, NULL) < 0
		|| sigaction (SIGTERM, &act, NULL) < 0)
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

	if (sigaction (SIGINT, &act, NULL) < 0
		|| sigaction (SIGTERM, &act, NULL) < 0)
		return false;

	return sigaction (SIGINT, &act, NULL) == 0
		   && sigaction (SIGTERM, &act, NULL) == 0
		   && sigaction (SIGHUP, &act, NULL) == 0;
}

bool
fh_master_read_config (struct fh_master *master)
{
#ifndef FHTTPD_MAIN_CONFIG_FILE
	#define FHTTPD_MAIN_CONFIG_FILE "/etc/freehttpd/fhttpd.conf"
#endif

	const char *config_file = FHTTPD_MAIN_CONFIG_FILE;

	struct fh_conf_parser *parser = fh_conf_parser_create (config_file);

	if (!parser)
	{
		fh_pr_err ("Failed to read configuration file: %s: %s", config_file,
				   strerror (errno));
		return false;
	}

	if (fh_conf_parser_read (parser) < 0)
	{
		fh_pr_err ("Failed to read configuration file: %s: %s", config_file,
				   strerror (errno));
		fh_conf_parser_destroy (parser);
		return false;
	}

	struct fh_config *config = fh_conf_process (parser, NULL, NULL);

	if (!config)
	{
		fh_pr_err ("Failed to parse configuration file:");
		fh_conf_parser_print_error (parser);
		fh_conf_parser_destroy (parser);
		return false;
	}

	master->config = config;
	fh_conf_parser_destroy (parser);

	fh_pr_info ("Read configuration file successfully");
	fh_conf_print (config, 0);

	return true;
}

bool
fh_master_load_modules (struct fh_master *master)
{
	master->module_manager = fh_module_manager_create ();

	if (!master->module_manager)
		return false;

	return fh_module_manager_load (master->module_manager);
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
			struct fh_config *config = master->config;
			fh_module_manager_free (master->module_manager);
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
