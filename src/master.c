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
#include <sys/shm.h>
#include <sys/wait.h>

#define FHTTPD_LOG_MODULE_NAME "master"

#include "compat.h"
#include "log.h"
#include "server.h"
#include "utils.h"
#include "worker.h"

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#ifdef FHTTPD_ENABLE_SYSTEMD
	#include <systemd/sd-daemon.h>
#endif /* FHTTPD_ENABLE_SYSTEMD */

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
	master->worker_stats = calloc (worker_count, sizeof (struct fhttpd_notify_stat));

	if (!master->workers || !master->worker_pipes || !master->worker_stats)
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

static bool
fhttpd_master_process_notification (struct fhttpd_master *master)
{
#ifdef FHTTPD_ENABLE_SYSTEMD
	bool stats_updated = false;
#endif /* FHTTPD_ENABLE_SYSTEMD */

	for (size_t i = 0; i < master->worker_count; i++)
	{
		uint8_t type = 0;

		if (read (master->worker_pipes[i][0], &type, sizeof type) != (ssize_t) sizeof type)
		{
			if (errno == EAGAIN || errno == EINTR)
				continue;

			return false;
		}

		switch (type)
		{
			case FHTTPD_NOTIFY_STAT:
				{
					struct fhttpd_notify_stat stat;

					if (read (master->worker_pipes[i][0], &stat, sizeof stat) != (ssize_t) sizeof stat)
					{
						if (errno == EAGAIN || errno == EINTR)
							continue;

						return false;
					}

					master->worker_stats[i] = stat;
#ifdef FHTTPD_ENABLE_SYSTEMD
					stats_updated = true;
#endif /* FHTTPD_ENABLE_SYSTEMD */
				}
				break;

			default:
				fhttpd_log_error ("Unknown notification type: %u", type);
				break;
		}
	}

#ifdef FHTTPD_ENABLE_SYSTEMD
	if (stats_updated)
	{
		uint64_t total_connection_count = 0, current_connection_count = 0;

		for (size_t i = 0; i < master->worker_count; i++)
		{
			total_connection_count += master->worker_stats[i].total_connection_count;
			current_connection_count += master->worker_stats[i].current_connection_count;
		}

		if (current_connection_count == 0 && total_connection_count == 0)
			sd_notify (0, "STATUS=Waiting for incoming connections");
		else if (current_connection_count == 0 && total_connection_count > 0)
			sd_notifyf (0, "STATUS=Total connections handled: %lu", total_connection_count);
		else
			sd_notifyf (0, "STATUS=Handling %lu connections, handled %lu total so far", current_connection_count,
						total_connection_count);

		fhttpd_log_debug ("systemd stats updated");
	}
#endif /* FHTTPD_ENABLE_SYSTEMD */

	return true;
}

bool
fhttpd_master_start (struct fhttpd_master *master)
{
	if (!fhttpd_master_spawn_workers (master))
		return false;

#ifdef FHTTPD_ENABLE_SYSTEMD
	sd_notify (0, "READY=1");
	sd_notify (0, "STATUS=Ready to handle connections");
#endif /* FHTTPD_ENABLE_SYSTEMD */

	while (true)
	{
		if (flag_terminate)
		{
#ifdef FHTTPD_ENABLE_SYSTEMD
			sd_notify (0, "STOPPING=1");
#endif /* FHTTPD_ENABLE_SYSTEMD */

			if (master)
				fhttpd_master_destroy (master);

			exit (0);
		}

		if (flag_reload_config)
		{
#ifdef FHTTPD_ENABLE_SYSTEMD
			sd_notify (0, "RELOADING=1");
#endif /* FHTTPD_ENABLE_SYSTEMD */

			if (!fhttpd_master_reload_config (master))
			{
				flag_reload_config = false;
#ifdef FHTTPD_ENABLE_SYSTEMD
				sd_notify (0, "READY=1");
				sd_notify (0, "STATUS=Failed to reload configuration");
#endif /* FHTTPD_ENABLE_SYSTEMD */
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

#ifdef FHTTPD_ENABLE_SYSTEMD
			sd_notify (0, "STATUS=Sent SIGQUIT to workers");
#endif /* FHTTPD_ENABLE_SYSTEMD */
			fhttpd_log_info ("Sent SIGQUIT to workers");

			for (size_t i = 0; i < master->worker_count; i++)
			{
				pid_t worker_pid = master->workers[i];
				waitpid (worker_pid, NULL, 0);
				fhttpd_log_info ("Worker %d terminated", worker_pid);
			}

			free (master->worker_stats);
			free (master->worker_pipes);
			free (master->workers);

			master->worker_count = 0;
			master->worker_pipes = NULL;
			master->workers = NULL;

			if (!fhttpd_master_spawn_workers (master))
			{
#ifdef FHTTPD_ENABLE_SYSTEMD
				sd_notify (0, "READY=1");
				sd_notify (0, "STATUS=Failed to re-spawn workers");
#endif /* FHTTPD_ENABLE_SYSTEMD */
				return false;
			}

			flag_reload_config = false;
#ifdef FHTTPD_ENABLE_SYSTEMD
			sd_notify (0, "READY=1");
			sd_notify (0, "STATUS=Configuration reloaded");
#endif /* FHTTPD_ENABLE_SYSTEMD */
		}

		fhttpd_master_process_notification (master);
		sleep (10);
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

	free (master->worker_stats);
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
