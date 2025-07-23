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
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define FH_LOG_MODULE_NAME "server"

#include "compat.h"
#include "conf.h"
#include "conn.h"
#include "event/accept.h"
#include "event/recv.h"
#include "event/send.h"
#include "hash/itable.h"
#include "log/log.h"
#include "router/router.h"
#include "server.h"
#include "module.h"

#define FH_SERVER_MAX_EVENTS 128

struct fh_server *
fh_server_create (struct fh_config *config, struct fh_module_manager *module_manager)
{
	struct fh_server *server = calloc (1, sizeof (struct fh_server));

	if (!server)
		return NULL;

	server->config = config;
	server->module_manager = module_manager;
	server->host_configs = config->hosts;
	server->sockfd_table = itable_create (0);

	if (!server->sockfd_table)
	{
		free (server);
		return NULL;
	}

	server->connections = itable_create (0);

	if (!server->connections)
	{
		itable_destroy (server->sockfd_table);
		free (server);
		return NULL;
	}

	server->xpoll_fd = xpoll_create ();

	if (server->xpoll_fd < 0)
	{
		itable_destroy (server->connections);
		itable_destroy (server->sockfd_table);
		free (server);
		return NULL;
	}

	server->router = calloc (1, sizeof (*server->router));

	if (!server->router || !fh_router_init (server->router, server))
	{
		xpoll_destroy (server->xpoll_fd);
		itable_destroy (server->connections);
		strtable_destroy (server->host_configs);
		itable_destroy (server->sockfd_table);
		free (server->router);
		free (server);
		return NULL;
	}

	return server;
}

void
fh_server_destroy (struct fh_server *server)
{
	for_each_itable_entry (server->sockfd_table, entry)
	{
		free (entry->data);
	}

	for_each_itable_entry (server->connections, entry)
	{
		fh_conn_destroy (entry->data);
	}

	itable_destroy (server->connections);
	itable_destroy (server->sockfd_table);
	xpoll_destroy (server->xpoll_fd);
	fh_module_manager_free (server->module_manager);
	fh_conf_free (server->config);
	fh_router_free (server->router);
	free (server->router);
	free (server);
}

static bool
fh_server_index_config (struct fh_server *server)
{
	for (struct strtable_entry *entry = server->host_configs->head; entry;
			 entry = entry->next)
	{
		struct fh_config_host *config = entry->data;
		struct fh_bound_addr *addr = &config->addr;

		if (addr->port == 80 || addr->port == 443)
			strtable_set (server->host_configs, addr->hostname, config);

		strtable_set (server->host_configs, addr->full_hostname, config);
		fh_pr_debug ("Loaded virtual host: %s", addr->full_hostname);
	}

	return true;
}

bool
fh_server_listen (struct fh_server *server)
{
	const size_t socket_cap = FH_SERVER_MAX_SOCKETS;
	uint16_t ports[socket_cap];
	size_t port_count = 0;

	for (struct strtable_entry *entry = server->host_configs->head; entry;
			 entry = entry->next)
	{
		struct fh_config_host *config = entry->data;
		struct fh_bound_addr *addr = &config->addr;

		for (size_t k = 0; k < port_count; k++)
		{
			if (ports[k] == addr->port)
				goto skip_addr;
		}

		if (port_count >= socket_cap)
			return false;

		ports[port_count++] = addr->port;
		skip_addr:
			continue;
	}

	for (size_t i = 0; i < port_count; i++)
	{
		fd_t sockfd = socket (AF_INET, SOCK_STREAM, 0);

		if (sockfd < 0)
			return false;

		int opt = 1;

		if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt) < 0
			|| setsockopt (sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof opt) < 0)
		{
			close (sockfd);
			return false;
		}

		struct timeval tv;

		tv.tv_sec = 10;
		tv.tv_usec = 0;

		if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) < 0
			|| setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv) < 0)
		{
			close (sockfd);
			return false;
		}

		struct sockaddr_in *in = calloc (1, sizeof (*in));

		if (!in)
		{
			close (sockfd);
			return false;
		}

		in->sin_family = AF_INET;
		in->sin_port = htons (ports[i]);
		in->sin_addr.s_addr = INADDR_ANY;

		if (bind (sockfd, in, sizeof *in) < 0)
		{
			free (in);
			close (sockfd);
			return false;
		}

		if (listen (sockfd, SOMAXCONN) < 0)
		{
			free (in);
			close (sockfd);
			return false;
		}

		if (!xpoll_add (server->xpoll_fd, sockfd, XPOLLIN, O_NONBLOCK))
		{
			free (in);
			close (sockfd);
			return false;
		}

		if (!itable_set (server->sockfd_table, (uint64_t) sockfd, in))
		{
			free (in);
			close (sockfd);
			xpoll_del (server->xpoll_fd, sockfd, XPOLLIN);
			return false;
		}

		fh_pr_info ("Listening on 0.0.0.0:%u", ports[i]);
	}

	return fh_server_index_config (server);
}

void
fh_server_loop (struct fh_server *server)
{
	size_t errors = 0;

	for (;;)
	{
		if (server->should_exit)
			return;

		xevent_t events[FH_SERVER_MAX_EVENTS];
		int nfds = xpoll_wait (server->xpoll_fd, events, FH_SERVER_MAX_EVENTS, -1);

		if (nfds < 0)
		{
			if (would_interrupt ())
				continue;

			if (errors >= 5)
			{
				fh_pr_emerg ("Too many xpoll_wait() errors occurred, not retrying: %s", strerror (errno));
				return;
			}

			errors++;
			continue;
		}

		errors = 0;

		for (int i = 0; i < nfds; i++)
		{
			uint32_t evflags = events[i].events;
			fd_t fd = events[i].data.fd;
			struct sockaddr_in *server_addr;

			if ((server_addr = itable_get (server->sockfd_table, fd)))
			{
				event_accept (server, &events[i], server_addr);
				continue;
			}

			if (evflags & XPOLLERR)
			{
				int err = xpoll_get_error (server->xpoll_fd, &events[i], fd);
				struct fh_conn *conn = itable_get (server->connections, (uint64_t) fd);

				if (!conn)
				{
					fh_pr_debug ("Socket I/O error occurred: Socket #%d: %s", fd, strerror (err));
					close (fd);
					continue;
				}

				fh_pr_debug ("Socket I/O error occurred: Connection #%lu: %s", conn->id, strerror (err));
				fh_server_close_conn (server, conn);
				continue;
			}

			if (evflags & XPOLLIN)
			{
				if (!event_recv (server, &events[i]))
					fh_pr_err ("recv event handler failed: %s", strerror (errno));

				continue;
			}
			else if (evflags & XPOLLOUT)
			{
				if (!event_send (server, &events[i]))
					fh_pr_err ("send event handler failed: %s", strerror (errno));

				continue;
			}

			if (evflags & XPOLLHUP)
			{
				/* event_hup */
				continue;
			}
		}
	}
}

void
fh_server_close_conn (struct fh_server *server, struct fh_conn *conn)
{
	itable_remove (server->connections, conn->client_sockfd);
	xpoll_del (server->xpoll_fd, conn->client_sockfd, XPOLLIN | XPOLLOUT);
	fh_conn_destroy (conn);
}
