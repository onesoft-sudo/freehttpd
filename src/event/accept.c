#define _GNU_SOURCE

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#define FHTTPD_LOG_MODULE_NAME "accept"

#include "accept.h"
#include "core/conn.h"
#include "log/log.h"
#include "utils/utils.h"

bool
fh_event_accept (struct fhttpd_server *server, const xevent_t *event)
{
	fd_t sockfd = event->data.fd;
	fd_t client_sockfd;
	struct sockaddr_in client_addr = { 0 };
	socklen_t client_addr_len = sizeof client_addr;

#ifdef __linux__
	client_sockfd = accept4 (sockfd, (struct sockaddr *) &client_addr, &client_addr_len, SOCK_NONBLOCK);

	if (client_sockfd < 0)
		return false;
#else  /* not __linux__ */
	client_sockfd = accept (sockfd, (struct sockaddr *) &client_addr, &client_addr_len);

	if (client_sockfd < 0)
		return false;

	if (!fd_set_nonblocking (client_sockfd))
	{
		close (client_sockfd);
		return false;
	}
#endif /* __linux__ */

	char client_ip[INET_ADDRSTRLEN] = { 0 };

	inet_ntop (AF_INET, &client_addr.sin_addr, client_ip, sizeof (client_ip));
	fhttpd_wclog_info ("Accepted connection from: %s:%d", client_ip, ntohs (client_addr.sin_port));

	struct fh_conn *conn = fhttpd_server_acquire_conn (server);

	if (!conn)
		conn = fh_conn_create (server->last_connection_id++, client_sockfd);

	if (!conn)
	{
		fhttpd_wclog_error ("Connection pool is full or cannot allocate memory");
		close (client_sockfd);
		return false;
	}

	bool is_heap = conn->is_heap;

	if (!is_heap && !fh_conn_init (conn, server->last_connection_id++, client_sockfd))
	{
		fhttpd_wclog_error ("Failed to allocate memory");
		fhttpd_server_release_conn (server, conn);
		close (client_sockfd);
		return false;
	}

	if (!xpoll_add (server->xpoll, client_sockfd, XPOLLIN | XPOLLHUP | XPOLLET, 0))
	{
		fh_conn_close (conn);

		if (!is_heap)
			fhttpd_server_release_conn (server, conn);

		return false;
	}

	struct fhttpd_addrinfo *server_addr = itable_get (server->sockaddr_in_table, (uint64_t) sockfd);

	if (!server_addr)
	{
		fhttpd_wclog_debug ("No server addr found for socket: %d", sockfd);
		
		fh_conn_close (conn);

		if (!is_heap)
			fhttpd_server_release_conn (server, conn);

		return false;
	}

	conn->config = &server->config->hosts[server->config->default_host_index];
	conn->protocol = FHTTPD_PROTOCOL_UNKNOWN;
	conn->extra->port = server_addr->port;

	if (!itable_set (server->connections, (uint64_t) client_sockfd, conn))
	{
		fh_conn_close (conn);

		if (!is_heap)
			fhttpd_server_release_conn (server, conn);

		xpoll_del (server->xpoll, client_sockfd, XPOLLIN);
		return false;
	}

	if (is_heap)
		fhttpd_wclog_debug ("connection %lu: Using heap", conn->id);

	return true;
}
