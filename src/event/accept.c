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
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define FH_LOG_MODULE_NAME "event/accept"

#include "accept.h"
#include "compat.h"
#include "core/conn.h"
#include "core/server.h"
#include "log/log.h"

bool
event_accept (struct fh_server *server, const xevent_t *ev_info, const struct sockaddr_in *server_addr)
{
	const fd_t sockfd = ev_info->data.fd;
	size_t errors = 0;
	uint32_t fdflags = 0;

	for (;;)
	{
		struct sockaddr_in client_addr = { 0 };
		socklen_t client_addr_len = sizeof client_addr;

#if defined(FH_PLATFORM_LINUX)
		fd_t client_sockfd = accept4 (sockfd, &client_addr, &client_addr_len, SOCK_NONBLOCK);
#elif defined(FH_PLATFORM_BSD)
		fd_t client_sockfd = accept (sockfd, &client_addr, &client_addr_len);
		fdflags = O_NONBLOCK;
#endif

		if (client_sockfd < 0)
		{
			if (errno == EINTR)
				continue;

			if (would_block ())
				return true;

			fh_pr_err ("accept() syscall failed: %s", strerror (errno));

			if (errors >= 5)
				return false;

			errors++;
			continue;
		}

		errors = 0;

		char ip[INET_ADDRSTRLEN] = { 0 };
		inet_ntop (AF_INET, &client_addr.sin_addr.s_addr, ip, sizeof ip);
		uint16_t port = ntohs (client_addr.sin_port);

		fh_pr_debug ("Accepted new connection from %s:%u", ip, port);

		struct fh_conn *conn = fh_conn_create (client_sockfd, &client_addr, server_addr);

		if (!conn)
		{
			close (client_sockfd);
			fh_pr_err ("Memory allocation failed");
			continue;
		}

		if (!itable_set (server->connections, (uint64_t) client_sockfd, conn))
		{
			fh_conn_destroy (conn);
			fh_pr_err ("Hash table set operation failed");
			continue;
		}

		if (!xpoll_add (server->xpoll_fd, client_sockfd, XPOLLIN | XPOLLET | XPOLLHUP, fdflags))
		{
			fh_server_close_conn (server, conn);
			fh_pr_err ("xpoll_add() operation failed");
			continue;
		}

		struct fh_config_host *config = server->config->default_host_config;
		struct fh_bound_addr *addr = &config->addr;

		conn->extra->host = addr->full_hostname;
		conn->extra->port = addr->port;
		conn->extra->host_len = addr->hostname_len;
		conn->extra->full_host_len = addr->full_hostname_len;
		conn->config = config;

		fh_pr_info ("Connection established with %s:%u", ip, port);
	}

	return true;
}
