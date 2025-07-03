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
#include "event/accept.h"
#include "event/recv.h"
#include "hash/itable.h"
#include "log/log.h"
#include "server.h"
#include "conn.h"

#define FH_SERVER_MAX_EVENTS 128

struct fh_server *
fh_server_create (struct fhttpd_config *config)
{
	struct fh_server *server = calloc (1, sizeof (struct fh_server));

	if (!server)
		return NULL;

	server->config = config;
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
	fhttpd_conf_free_config (server->config);
	free (server);
}

bool
fh_server_listen (struct fh_server *server)
{
	const size_t socket_cap = FH_SERVER_MAX_SOCKETS;
	uint16_t ports[socket_cap];
	size_t port_count = 0;

	for (size_t i = 0; i < server->config->host_count; i++)
	{
		for (size_t j = 0; j < server->config->hosts[i].bound_addr_count; j++)
		{
			struct fhttpd_bound_addr *addr = server->config->hosts[i].bound_addrs;

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

	return true;
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

			if (evflags & XPOLLIN)
			{
				if (!event_recv (server, &events[i]))
					fh_pr_err ("recv event handler failed: %s", strerror (errno));
				
				continue;
			}
			else if (evflags & XPOLLOUT)
			{
				/* event_send */
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
