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

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define FHTTPD_LOG_MODULE_NAME "server"

#include "conf.h"
#include "conn.h"
#include "http/protocol.h"
#include "log/log.h"
#include "modules/autoindex.h"
#include "server.h"
#include "types.h"
#include "utils/itable.h"
#include "utils/utils.h"
#include "event/xpoll.h"
#include "event/accept.h"
#include "event/recv.h"
#include "event/send.h"

#ifdef HAVE_RESOURCES
	#include "resources.h"
#endif /* HAVE_RESOURCES */

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef FHTTPD_ENABLE_SYSTEMD
	#include <systemd/sd-daemon.h>
#endif /* FHTTPD_ENABLE_SYSTEMD */

#define FHTTPD_DEFAULT_BACKLOG SOMAXCONN
#define FHTTPD_MAX_EVENTS             64

struct fhttpd_server *
fhttpd_server_create (const struct fhttpd_master *master, struct fhttpd_config *config, fd_t pipe_fd[static 2])
{
	struct fhttpd_server *server = calloc (1, sizeof (struct fhttpd_server));

	if (!server)
		return NULL;

	memcpy (server->pipe_fd, pipe_fd, sizeof server->pipe_fd);

	fd_set_nonblocking (pipe_fd[1]);

	server->master_pid = master->pid;
	server->pid = getpid ();
	server->xpoll = xpoll_create ();
	server->listen_fds = NULL;

	if (server->xpoll < 0)
	{
		free (server);
		return NULL;
	}

	server->connections = itable_create (0);

	if (!server->connections)
	{
		xpoll_destroy (server->xpoll);
		free (server);
		return NULL;
	}

	server->sockaddr_in_table = itable_create (0);

	if (!server->sockaddr_in_table)
	{
		itable_destroy (server->connections);
		xpoll_destroy (server->xpoll);
		free (server);
		return NULL;
	}

	server->host_config_table = strtable_create (0);

	if (!server->host_config_table)
	{
		itable_destroy (server->sockaddr_in_table);
		itable_destroy (server->connections);
		xpoll_destroy (server->xpoll);
		free (server);
		return NULL;
	}

	server->timer_fd = -1;
	server->config = config;

	return server;
}

static bool
fhttpd_server_create_sockets (struct fhttpd_server *server)
{
	uint16_t *ports = NULL;
	size_t port_count = 0;

	for (size_t i = 0; i < server->config->host_count; i++)
	{
		const struct fhttpd_config_host *host = &server->config->hosts[i];

		for (size_t j = 0; j < host->bound_addr_count; j++)
		{
			const struct fhttpd_bound_addr *addr = &host->bound_addrs[j];

			for (size_t k = 0; k < port_count; k++)
			{
				if (ports[k] == addr->port)
					goto fhttpd_server_create_sockets_addr_end;
			}

			uint16_t *new_ports = realloc (ports, sizeof (*ports) * (port_count + 1));

			if (!new_ports)
			{
				free (ports);
				return false;
			}

			ports = new_ports;
			ports[port_count++] = addr->port;

		fhttpd_server_create_sockets_addr_end:
			continue;
		}
	}

	for (size_t i = 0; i < port_count; i++)
	{
		int port = ports[i];
		int sockfd = socket (AF_INET, SOCK_STREAM, 0);

		if (sockfd < 0)
		{
			free (ports);
			return false;
		}

		int opt = 1;
		struct timeval tv = { 0 };

		tv.tv_sec = 10;

		setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
		setsockopt (sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof (opt));
		setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (tv));
		setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof (tv));

		struct sockaddr_in addr = {
			.sin_family = AF_INET,
			.sin_port = htons (port),
			.sin_addr.s_addr = INADDR_ANY,
		};

		if (bind (sockfd, (struct sockaddr *) &addr, sizeof (addr)) < 0)
		{
			close (sockfd);
			free (ports);
			return false;
		}

		if (listen (sockfd, FHTTPD_DEFAULT_BACKLOG) < 0)
		{
			close (sockfd);
			free (ports);
			return false;
		}

		fd_set_nonblocking (sockfd);
		server->listen_fds = realloc (server->listen_fds, sizeof (fd_t) * ++server->listen_fd_count);

		if (!server->listen_fds)
		{
			close (sockfd);
			free (ports);
			return false;
		}

		server->listen_fds[server->listen_fd_count - 1] = sockfd;

		if (!xpoll_add (server->xpoll, sockfd, XPOLLIN, 0))
		{
			close (sockfd);
			free (ports);
			return false;
		}

		struct fhttpd_addrinfo *srv_addr = malloc (sizeof (struct fhttpd_addrinfo));

		if (!srv_addr)
		{
			close (sockfd);
			free (ports);
			return false;
		}

		srv_addr->addr = addr;
		srv_addr->addr_len = sizeof (addr);
		srv_addr->port = port;
		srv_addr->sockfd = sockfd;
		inet_ntop (AF_INET, &addr.sin_addr, srv_addr->host_addr, sizeof (srv_addr->host_addr));

		if (!itable_set (server->sockaddr_in_table, sockfd, srv_addr))
		{
			free (srv_addr);
			close (sockfd);
			free (ports);
			return false;
		}
	}

	free (ports);
	return true;
}

void
fhttpd_server_config_host_map (struct fhttpd_server *server)
{
	for (size_t i = 0; i < server->config->host_count; i++)
	{
		struct fhttpd_config_host *host = &server->config->hosts[i];

		for (size_t j = 0; j < host->bound_addr_count; j++)
			strtable_set (server->host_config_table, host->bound_addrs[j].full_hostname, host);
	}

	fhttpd_wclog_debug ("Mapped %zu host configurations", server->config->host_count);
}

bool
fhttpd_server_prepare (struct fhttpd_server *server)
{
	fhttpd_server_config_host_map (server);
	return fhttpd_server_create_sockets (server);
}

void
fhttpd_server_destroy (struct fhttpd_server *server)
{
	if (!server)
		return;

	close (server->pipe_fd[0]);
	close (server->pipe_fd[1]);

	if (server->host_config_table)
		strtable_destroy (server->host_config_table);

	if (server->timer_fd >= 0)
	{
		close (server->timer_fd);
		server->timer_fd = -1;
	}

	if (server->listen_fds)
	{
		for (size_t i = 0; i < server->listen_fd_count; i++)
		{
			close (server->listen_fds[i]);
		}

		free (server->listen_fds);
	}

	if (server->xpoll >= 0)
		xpoll_destroy (server->xpoll);

	if (server->connections)
	{
		struct itable_entry *entry = server->connections->head;

		while (entry)
		{
			struct fh_conn *conn = entry->data;

			if (conn)
				fh_conn_close (conn);

			entry = entry->next;
		}

		itable_destroy (server->connections);
	}

	if (server->sockaddr_in_table)
	{
		struct itable_entry *entry = server->sockaddr_in_table->head;

		while (entry)
		{
			struct fhttpd_addrinfo *addr = entry->data;

			if (addr)
				free (addr);

			entry = entry->next;
		}

		itable_destroy (server->sockaddr_in_table);
	}

	fhttpd_conf_free_config (server->config);
	free (server);
}

_noreturn void
fhttpd_server_loop (struct fhttpd_server *server)
{
	size_t errors = 0;

	for (;;)
	{
		if (server->flag_terminate)
			exit (EXIT_SUCCESS);

		xevent_t events[FHTTPD_MAX_EVENTS];
		int nfds = xpoll_wait (server->xpoll, events, FHTTPD_MAX_EVENTS, -1);

		if (nfds < 0)
		{
			if (errno == EAGAIN || errno == EINTR)
				continue;

			errors++;
			fhttpd_wclog_error ("xpoll_wait() returned an error: %s", strerror (errno));

			if (errors >= 5)
				exit (EXIT_FAILURE);

			continue;
		}

		if (nfds == 0 && server->flag_clean_quit)
			exit (EXIT_SUCCESS);

		errors = 0;

		for (size_t i = 0; i < (size_t) nfds; i++)
		{
			if (itable_contains (server->sockaddr_in_table, (uint64_t) events[i].data.fd))
			{
				if (server->flag_terminate || server->flag_clean_quit)
					continue;

				if (!fh_event_accept (server, &events[i]))
					fhttpd_wclog_error ("accept failed: %s", strerror (errno));

				continue;
			}

			uint32_t ev = events[i].events;
			struct fh_conn *conn = itable_get (server->connections, events[i].data.fd);

			if (!conn)
			{
				fhttpd_wclog_warning ("Socket %d doesn't have a connection", events[i].data.fd);
				continue;
			}

			uint64_t id = conn->id;

			if (ev & XPOLLHUP)
			{
				fhttpd_wclog_warning ("Client HUP received");
				fhttpd_server_conn_close (server, conn);
				continue;
			}

			if (ev & XPOLLIN)
			{
				if (!fh_event_recv (server, conn))
				{
					fhttpd_wclog_error ("connection %lu: recv failed: %s", id, strerror (errno));
					continue;
				}
			}
			else if (ev & XPOLLOUT)
			{
				if (!fh_event_send (server, conn))
				{
					fhttpd_wclog_error ("connection %lu: send failed: %s", id, strerror (errno));
					continue;
				}
			}
		}
	}
}

bool
fhttpd_server_conn_close (struct fhttpd_server *server, struct fh_conn *conn)
{
	uint64_t id = conn->id;
	fd_t sockfd = conn->client_sockfd;

	itable_remove (server->connections, conn->client_sockfd);
	xpoll_del (server->xpoll, conn->client_sockfd, XPOLLIN | XPOLLOUT);
	fh_conn_close (conn);

	fhttpd_wclog_debug ("connection %lu: socket %d closed and deallocated", sockfd, id);
	return true;
}