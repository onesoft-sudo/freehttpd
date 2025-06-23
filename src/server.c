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

#include "autoindex.h"
#include "conf.h"
#include "connection.h"
#include "itable.h"
#include "log.h"
#include "loop.h"
#include "protocol.h"
#include "server.h"
#include "types.h"
#include "utils.h"

#ifdef HAVE_RESOURCES
#include "resources.h"
#endif

#define FHTTPD_DEFAULT_BACKLOG SOMAXCONN
#define MAX_EVENTS 64

struct fhttpd_server *
fhttpd_server_create (const struct fhttpd_master *master, struct fhttpd_config *config)
{
	struct fhttpd_server *server = calloc (1, sizeof (struct fhttpd_server));

	if (!server)
		return NULL;

	server->master_pid = master->pid;
	server->pid = getpid ();
	server->epoll_fd = epoll_create1 (0);
	server->listen_fds = NULL;

	if (server->epoll_fd < 0)
	{
		free (server);
		return NULL;
	}

	server->connections = itable_create (0);

	if (!server->connections)
	{
		close (server->epoll_fd);
		free (server);
		return NULL;
	}

	server->sockaddr_in_table = itable_create (0);

	if (!server->sockaddr_in_table)
	{
		itable_destroy (server->connections);
		close (server->epoll_fd);
		free (server);
		return NULL;
	}

	server->host_config_table = strtable_create (0);

	if (!server->host_config_table)
	{
		itable_destroy (server->sockaddr_in_table);
		itable_destroy (server->connections);
		close (server->epoll_fd);
		free (server);
		return NULL;
	}

	server->timer_fd = -1;
	server->config = config;

	return server;
}

static bool
fhttpd_fd_set_nonblocking (int fd)
{
	int flags = fcntl (fd, F_GETFL);

	if (flags < 0)
		return false;

	if (fcntl (fd, F_SETFL, flags | O_NONBLOCK) < 0)
		return false;

	return true;
}

static bool
fhttpd_epoll_ctl_add (struct fhttpd_server *server, fd_t fd, uint32_t events)
{
	struct epoll_event ev = { 0 };

	ev.events = events;
	ev.data.fd = fd;

	if (epoll_ctl (server->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
		return false;

	return true;
}
static bool
fhttpd_epoll_ctl_mod (struct fhttpd_server *server, fd_t fd, uint32_t events)
{
	struct epoll_event ev = { 0 };

	ev.events = events;
	ev.data.fd = fd;

	if (epoll_ctl (server->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
		return false;

	return true;
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

		fhttpd_fd_set_nonblocking (sockfd);
		server->listen_fds = realloc (server->listen_fds, sizeof (fd_t) * ++server->listen_fd_count);

		if (!server->listen_fds)
		{
			close (sockfd);
			free (ports);
			return false;
		}

		server->listen_fds[server->listen_fd_count - 1] = sockfd;

		if (!fhttpd_epoll_ctl_add (server, sockfd, EPOLLIN))
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

bool
fhttpd_server_prepare (struct fhttpd_server *server)
{

	for (size_t i = 0; i < server->config->host_count; i++)
	{
		struct fhttpd_config_host *host = &server->config->hosts[i];

		for (size_t j = 0; j < host->bound_addr_count; j++)
			strtable_set (server->host_config_table, host->bound_addrs[j].full_hostname, host);
	}

	fhttpd_wclog_debug ("Mapped all host configs");
	return fhttpd_server_create_sockets (server);
}

void
fhttpd_server_destroy (struct fhttpd_server *server)
{
	if (!server)
		return;

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

	if (server->epoll_fd >= 0)
		close (server->epoll_fd);

	if (server->connections)
	{
		struct itable_entry *entry = server->connections->head;

		while (entry)
		{
			struct fhttpd_connection *conn = entry->data;

			if (conn)
				fhttpd_connection_close (conn);

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

static struct fhttpd_connection *
fhttpd_server_new_connection (struct fhttpd_server *server, fd_t client_sockfd)
{
	struct fhttpd_connection *conn = fhttpd_connection_create (server->last_connection_id + 1, client_sockfd);

	if (!conn)
		return NULL;

	if (!itable_set (server->connections, client_sockfd, conn))
	{
		fhttpd_connection_close (conn);
		return NULL;
	}

	server->last_connection_id++;
	return conn;
}

static bool
fhttpd_server_free_connection (struct fhttpd_server *server, struct fhttpd_connection *conn)
{
	fd_t fd = conn->client_sockfd;

	if (itable_remove (server->connections, fd) != conn)
		return false;

	fhttpd_wclog_debug ("Connection #%lu is being deallocated", conn->id);
	fhttpd_connection_close (conn);

	if (epoll_ctl (server->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0)
		return false;

	close (fd);
	return true;
}

static bool
fhttpd_server_accept (struct fhttpd_server *server, size_t fd_index)
{
	if (server->config->sec_max_connections > 0 && server->connections->count >= server->config->sec_max_connections)
	{
		errno = EAGAIN;
		return false;
	}

	fd_t sockfd = server->listen_fds[fd_index];
	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof (client_addr);

#if defined(__linux__) || defined(__FreeBSD__)
	fd_t client_sockfd = accept4 (sockfd, (struct sockaddr *) &client_addr, &addrlen, SOCK_NONBLOCK);
#else
	fd_t client_sockfd = accept (sockfd, (struct sockaddr *) &client_addr, &addrlen);
#endif /* defined(__linux__) || defined(__FreeBSD__) */

	if (client_sockfd < 0)
		return errno == EAGAIN || errno == EWOULDBLOCK;

	struct fhttpd_addrinfo *srv_addr = itable_get (server->sockaddr_in_table, sockfd);

	if (!srv_addr)
	{
		fhttpd_wclog_error ("Failed to retrieve server address for socket %d", sockfd);
		close (client_sockfd);
		return false;
	}

	char ip[INET_ADDRSTRLEN];
	inet_ntop (AF_INET, &client_addr.sin_addr, ip, sizeof (ip));
	fhttpd_wclog_info ("Accepted connection from %s:%d", ip, ntohs (client_addr.sin_port));

#if !defined(__linux__) && !defined(__FreeBSD__)
	if (!fhttpd_fd_set_nonblocking (client_sockfd))
	{
		close (client_sockfd);
		return false;
	}
#endif /* !defined(__linux__) && !defined (__FreeBSD__) */

	if (!fhttpd_epoll_ctl_add (server, client_sockfd, EPOLLIN | EPOLLET | EPOLLHUP))
	{
		close (client_sockfd);
		return false;
	}

	struct fhttpd_connection *conn = fhttpd_server_new_connection (server, client_sockfd);

	if (!conn)
	{
		epoll_ctl (server->epoll_fd, EPOLL_CTL_DEL, client_sockfd, NULL);
		close (client_sockfd);
		return false;
	}

	conn->port = srv_addr->port;
	fhttpd_wclog_info ("Connection established with %s:%d [ID #%lu]", ip, ntohs (client_addr.sin_port), conn->id);
	return true;
}

static bool
fhttpd_server_add_default_headers (struct fhttpd_response *response)
{
	if (!response || response->headers.count > 0)
		return true;

	response->headers.list = malloc (sizeof (struct fhttpd_header) * 3);

	if (!response->headers.list)
		return false;

	response->headers.count = 0;

	time_t now = time (NULL);
	struct tm *tm = gmtime (&now);
	char date_buf[128] = { 0 };
	size_t date_buf_size = strftime (date_buf, sizeof (date_buf), "%a, %d %b %Y %H:%M:%S GMT", tm);

	if (!fhttpd_header_add_noalloc (&response->headers, 0, "Server", "freehttpd", 6, 9))
		return false;

	if (!fhttpd_header_add_noalloc (&response->headers, 1, "Connection", "close", 10, 5))
		return false;

	if (!fhttpd_header_add_noalloc (&response->headers, 2, "Date", date_buf, 4, date_buf_size))
		return false;

	return true;
}

static bool
fhttpd_process_file_request (struct fhttpd_server *server __attribute_maybe_unused__,
							 const struct fhttpd_config_host *config __attribute_maybe_unused__,
							 const struct fhttpd_request *request, struct fhttpd_response *response,
							 const char *filepath, const struct stat *st)
{
	response->status = FHTTPD_STATUS_OK;
	response->ready = true;
	response->body = NULL;
	response->body_len = st->st_size;
	response->fd = request->method == FHTTPD_METHOD_HEAD ? -1 : open (filepath, O_RDONLY | O_CLOEXEC);
	response->set_content_length = true;

	if (request->method != FHTTPD_METHOD_HEAD && response->fd < 0)
	{
		fhttpd_wclog_error ("Failed to open file '%s': %s", filepath, strerror (errno));
		response->status = FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
		response->use_builtin_error_response = true;
		return true;
	}

	char etag[128];
	int etag_len = snprintf (etag, sizeof (etag), "\"%lx%lx-%lx\"", (unsigned long) st->st_ino,
							 (unsigned long) st->st_mtime, (long) st->st_size);

	if (etag_len < 0 || (size_t) etag_len >= sizeof (etag))
	{
		fhttpd_wclog_error ("Failed to generate ETag for file '%s': %s", filepath, strerror (errno));
		response->status = FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
		response->use_builtin_error_response = true;
		if (response->fd >= 0)
			close (response->fd);
		return true;
	}

	fhttpd_header_add (&response->headers, "ETag", etag, 4, (size_t) etag_len);
	return true;
}

static bool
fhttpd_process_directory_request (struct fhttpd_server *server __attribute_maybe_unused__,
								  const struct fhttpd_config_host *config __attribute_maybe_unused__,
								  const struct fhttpd_request *request __attribute_maybe_unused__,
								  struct fhttpd_response *response, const char *filepath, size_t filepath_len,
								  const struct stat *st __attribute_maybe_unused__)
{
	return fhttpd_autoindex (request, response, filepath, filepath_len);
}

static bool
fhttpd_process_static_request (struct fhttpd_server *server, const struct fhttpd_config_host *config,
							   const struct fhttpd_request *request, struct fhttpd_response *response)
{
	const char *docroot = config->host_config->docroot;
	const size_t docroot_len = strlen (docroot);
	const size_t max_body_len = config->host_config->sec_max_response_body_size;

	response->ready = false;
	response->fd = -1;
	response->headers.list = NULL;
	response->headers.count = 0;

#ifndef NDEBUG
	if (!docroot)
	{
		fhttpd_wclog_error ("Document root is not set in the server configuration");
		return false;
	}
#endif /* NDEBUG */

	if (!fhttpd_server_add_default_headers (response))
	{
		fhttpd_wclog_error ("Failed to add default headers to response");
		return false;
	}

	if (request->method != FHTTPD_METHOD_GET && request->method != FHTTPD_METHOD_HEAD)
	{
		fhttpd_wclog_error ("Unsupported method: %d", request->method);
		response->ready = true;
		response->status = FHTTPD_STATUS_METHOD_NOT_ALLOWED;
		response->use_builtin_error_response = true;
		fhttpd_header_add (&response->headers, "Allow", "GET, HEAD", 5, 9);
		return true;
	}

	if (docroot_len + request->path_len > PATH_MAX)
	{
		fhttpd_wclog_error ("Document root length (%zu) + Path length (%zu) exceeds maximum path length (%d)",
							docroot_len, request->path_len, PATH_MAX);
		response->ready = true;
		response->status = FHTTPD_STATUS_REQUEST_URI_TOO_LONG;
		response->use_builtin_error_response = true;
		return true;
	}

	char filepath[PATH_MAX + 1] = { 0 };
	size_t filepath_len = docroot_len + request->path_len;
	struct stat st;

	memcpy (filepath, docroot, docroot_len);
	memcpy (filepath + docroot_len, request->path, request->path_len);

	if (filepath[PATH_MAX] != 0 || !path_normalize (filepath, filepath, &filepath_len))
	{
		fhttpd_wclog_error ("Failed to normalize file path: '%s'", filepath);
		response->ready = true;
		response->status = FHTTPD_STATUS_FORBIDDEN;
		response->use_builtin_error_response = true;
		return true;
	}

	if (filepath_len < docroot_len || strncmp (filepath, docroot, docroot_len) != 0)
	{
		fhttpd_wclog_error ("Normalized file path '%s' is outside of document root '%s'", filepath, docroot);
		response->ready = true;
		response->status = FHTTPD_STATUS_FORBIDDEN;
		response->use_builtin_error_response = true;
		return true;
	}

	fhttpd_wclog_debug ("Normalized file path: '%s'", filepath);

	if (lstat (filepath, &st) < 0)
	{
		int err = errno;
		fhttpd_wclog_error ("Failed to stat file '%s': %s", filepath, strerror (err));
		response->ready = true;
		response->status = err == ENOENT || err == ENOTDIR ? FHTTPD_STATUS_NOT_FOUND
						   : err == EACCES				   ? FHTTPD_STATUS_FORBIDDEN
														   : FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
		response->use_builtin_error_response = true;
		return true;
	}

	if (!S_ISREG (st.st_mode) && !S_ISDIR (st.st_mode))
	{
		fhttpd_wclog_error ("'%s' is not a regular file or directory", filepath);
		response->ready = true;
		response->status = FHTTPD_STATUS_FORBIDDEN;
		response->use_builtin_error_response = true;
		return true;
	}

	if ((size_t) st.st_size > max_body_len)
	{
		fhttpd_wclog_error ("File '%s' exceeds maximum body size of %zu bytes", filepath, max_body_len);
		response->ready = true;
		response->status = FHTTPD_STATUS_NOT_FOUND;
		response->use_builtin_error_response = true;
		return true;
	}

	if (access (filepath, R_OK) < 0)
	{
		fhttpd_wclog_error ("Access denied to file '%s': %s", filepath, strerror (errno));
		response->ready = true;
		response->status = FHTTPD_STATUS_FORBIDDEN;
		response->use_builtin_error_response = true;
		return true;
	}

	if (S_ISREG (st.st_mode))
		return fhttpd_process_file_request (server, config, request, response, filepath, &st);

	if (S_ISDIR (st.st_mode))
		return fhttpd_process_directory_request (server, config, request, response, filepath, filepath_len, &st);

	response->status = FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
	response->ready = true;
	response->use_builtin_error_response = true;

	return false;
}

static bool
fhttpd_process_request (struct fhttpd_server *server, struct fhttpd_connection *conn, size_t request_index,
						struct fhttpd_response *response)
{
	struct fhttpd_request *request = &conn->requests[request_index];
	fhttpd_wclog_info ("Processing request #%zu for connection #%lu", request_index, conn->id);
	struct fhttpd_config_host *config = strtable_get (server->host_config_table, conn->full_hostname);

	if (!config)
		config = strtable_get (server->host_config_table,
							   server->config->hosts[server->config->default_host_index].bound_addrs[0].full_hostname);

	if (!config || !config->host_config)
	{
		response->ready = true;
		response->status = FHTTPD_STATUS_INTERNAL_SERVER_ERROR;
		response->use_builtin_error_response = true;
		return true;
	}

	return fhttpd_process_static_request (server, config, request, response);
}

static loop_op_t
fhttpd_server_on_read_ready (struct fhttpd_server *server, fd_t client_sockfd)
{
	struct fhttpd_connection *conn = itable_get (server->connections, client_sockfd);

	if (!conn)
	{
		fhttpd_wclog_error ("Client data received, no connection found for socket: %d", client_sockfd);
		epoll_ctl (server->epoll_fd, EPOLL_CTL_DEL, client_sockfd, NULL);
		close (client_sockfd);
		return LOOP_OPERATION_NONE;
	}

	if (conn->protocol == FHTTPD_PROTOCOL_UNKNOWN)
	{
		if (!fhttpd_connection_detect_protocol (conn))
		{
			fhttpd_wclog_error ("Connection #%lu: Unable to detect protocol: %s", conn->id, strerror (errno));
			fhttpd_server_free_connection (server, conn);
			return LOOP_OPERATION_NONE;
		}

		if (conn->protocol != FHTTPD_PROTOCOL_UNKNOWN)
			fhttpd_wclog_info ("Connection #%lu: Protocol detected: %s", conn->id,
							   fhttpd_protocol_to_string (conn->protocol));
		else
			return LOOP_OPERATION_NONE;

		struct fhttpd_bound_addr *dfl_addr = &server->config->hosts[server->config->default_host_index].bound_addrs[0];

		switch (conn->protocol)
		{
			case FHTTPD_PROTOCOL_HTTP1x:
				http1_parser_ctx_init (&conn->proto.http1.http1_req_ctx);
				static_assert (sizeof (conn->proto.http1.http1_req_ctx.buffer) >= H2_PREFACE_SIZE,
							   "protobuf is smaller than H2 preface");
				memcpy (conn->proto.http1.http1_req_ctx.buffer, conn->buffers.protobuf, H2_PREFACE_SIZE);
				conn->proto.http1.http1_req_ctx.buffer_len = H2_PREFACE_SIZE;
				conn->mode = FHTTPD_CONNECTION_MODE_READ;
				conn->hostname = dfl_addr->hostname;
				conn->hostname_len = dfl_addr->hostname_len;
				conn->full_hostname = dfl_addr->full_hostname;
				conn->full_hostname_len = dfl_addr->full_hostname_len;
				fhttpd_wclog_info ("Connection #%lu: HTTP/1.x protocol initialized", conn->id);
				break;

			default:
				fhttpd_wclog_error ("Connection #%lu: Unsupported protocol: %d", conn->id, conn->protocol);
				fhttpd_server_free_connection (server, conn);
				return LOOP_OPERATION_NONE;
		}
	}

	switch (conn->protocol)
	{
		case FHTTPD_PROTOCOL_HTTP1x:
			if (!http1_parse (conn, &conn->proto.http1.http1_req_ctx))
			{
				fhttpd_wclog_error ("Connection #%lu: HTTP/1.x parsing failed", conn->id);
				conn->mode = FHTTPD_CONNECTION_MODE_WRITE;

				if (fhttpd_connection_defer_error_response (conn, 0, FHTTPD_STATUS_BAD_REQUEST))
					fhttpd_connection_send_response (conn, 0, NULL);

				fhttpd_server_free_connection (server, conn);
				return LOOP_OPERATION_NONE;
			}

			if (conn->proto.http1.http1_req_ctx.state == HTTP1_STATE_DONE)
			{
				fhttpd_wclog_info ("Connection #%lu: HTTP/1.x request parsed successfully", conn->id);

				if (!fhttpd_epoll_ctl_mod (server, conn->client_sockfd, EPOLLOUT | EPOLLHUP))
				{
					fhttpd_wclog_error ("Connection #%lu: Failed to modify epoll events: %s", conn->id,
										strerror (errno));
					fhttpd_server_free_connection (server, conn);
					return LOOP_OPERATION_NONE;
				}

				conn->mode = FHTTPD_CONNECTION_MODE_WRITE;

				struct fhttpd_request *requests
					= realloc (conn->requests, sizeof (struct fhttpd_request) * (conn->request_count + 1));

				if (!requests)
				{
					fhttpd_wclog_error ("Memory allocation failed");

					if (fhttpd_connection_defer_error_response (conn, 0, FHTTPD_STATUS_INTERNAL_SERVER_ERROR))
						fhttpd_connection_send_response (conn, 0, NULL);

					fhttpd_server_free_connection (server, conn);
					return LOOP_OPERATION_NONE;
				}

				conn->requests = requests;

				struct fhttpd_request *request = &conn->requests[conn->request_count++];

				request->conn = conn;
				request->protocol = conn->protocol;
				request->method = conn->proto.http1.http1_req_ctx.result.method;
				request->uri = conn->proto.http1.http1_req_ctx.result.uri;
				request->uri_len = conn->proto.http1.http1_req_ctx.result.uri_len;
				request->host = conn->proto.http1.http1_req_ctx.result.host;
				request->host_len = conn->proto.http1.http1_req_ctx.result.host_len;
				request->full_host = conn->proto.http1.http1_req_ctx.result.full_host;
				request->full_host_len = conn->proto.http1.http1_req_ctx.result.full_host_len;
				request->host_port = conn->proto.http1.http1_req_ctx.result.host_port;
				request->qs = conn->proto.http1.http1_req_ctx.result.qs;
				request->qs_len = conn->proto.http1.http1_req_ctx.result.qs_len;
				request->path = conn->proto.http1.http1_req_ctx.result.path;
				request->path_len = conn->proto.http1.http1_req_ctx.result.path_len;
				request->headers = conn->proto.http1.http1_req_ctx.result.headers;
				request->body = (uint8_t *) conn->proto.http1.http1_req_ctx.result.body;
				request->body_len = conn->proto.http1.http1_req_ctx.result.body_len;

				memcpy (conn->exact_protocol, conn->proto.http1.http1_req_ctx.result.version, 4);
				http1_parser_ctx_init (&conn->proto.http1.http1_req_ctx);
				conn->last_request_timestamp = get_current_timestamp ();

				if (conn->request_count == 1)
				{
					struct fhttpd_config_host *host_config
						= strtable_get (server->host_config_table, request->full_host);
					struct fhttpd_bound_addr *dfl_addr
						= &server->config->hosts[server->config->default_host_index].bound_addrs[0];
					bool known_host = host_config != NULL && conn->port == request->host_port;
					fhttpd_wclog_debug ("%s is%s a known host", request->full_host, known_host ? "" : " not");

					conn->hostname = known_host ? request->host : dfl_addr->hostname;
					conn->hostname_len = known_host ? request->host_len : dfl_addr->hostname_len;
					conn->full_hostname = known_host ? request->full_host : dfl_addr->full_hostname;
					conn->full_hostname_len = known_host ? request->full_host_len : dfl_addr->full_hostname_len;
				}
				else if (strcmp (conn->full_hostname, request->full_host) || conn->port != request->host_port)
				{
					fhttpd_wclog_debug (
						"Connection #%lu: Request #%zu: Host '%s' doesn't match with initial connection host '%s'",
						conn->id, conn->request_count - 1, request->full_host, conn->full_hostname);

					if (fhttpd_connection_defer_error_response (conn, 0, FHTTPD_STATUS_INTERNAL_SERVER_ERROR))
						fhttpd_connection_send_response (conn, 0, NULL);

					fhttpd_server_free_connection (server, conn);
					return LOOP_OPERATION_NONE;
				}

				fhttpd_wclog_info ("Connection #%lu: Accepted request #%zu", conn->id, conn->request_count - 1);
				return LOOP_OPERATION_NONE;
			}
			else if (conn->proto.http1.http1_req_ctx.state == HTTP1_STATE_RECV)
			{
				fhttpd_wclog_debug ("Connection #%lu: Waiting for more data in HTTP/1.x parser", conn->id);
				return LOOP_OPERATION_NONE;
			}
			else if (conn->proto.http1.http1_req_ctx.state == HTTP1_STATE_ERROR)
			{
				fhttpd_wclog_debug ("Connection #%lu: HTTP/1.x parser encountered an error", conn->id);
				conn->mode = FHTTPD_CONNECTION_MODE_WRITE;

				if (!fhttpd_epoll_ctl_mod (server, conn->client_sockfd, EPOLLOUT | EPOLLHUP))
				{
					fhttpd_wclog_error ("Connection #%lu: Failed to modify epoll events: %s", conn->id,
										strerror (errno));
					fhttpd_server_free_connection (server, conn);
					return LOOP_OPERATION_NONE;
				}

				if (fhttpd_connection_defer_error_response (conn,
															conn->request_count == 0 ? 0 : (conn->request_count - 1),
															FHTTPD_STATUS_INTERNAL_SERVER_ERROR))
					fhttpd_connection_send_response (conn, conn->request_count == 0 ? 0 : (conn->request_count - 1),
													 NULL);

				fhttpd_server_free_connection (server, conn);
				return LOOP_OPERATION_NONE;
			}
			break;

		default:
			fhttpd_wclog_error ("Connection #%lu: Unsupported protocol: %d", conn->id, conn->protocol);
			fhttpd_server_free_connection (server, conn);
			return LOOP_OPERATION_NONE;
	}

	return LOOP_OPERATION_NONE;
}

static loop_op_t
fhttpd_server_on_write_ready (struct fhttpd_server *server, fd_t client_sockfd)
{
	struct fhttpd_connection *conn = itable_get (server->connections, client_sockfd);

	if (!conn)
	{
		fhttpd_wclog_error ("No connection found for socket: %d", client_sockfd);
		epoll_ctl (server->epoll_fd, EPOLL_CTL_DEL, client_sockfd, NULL);
		close (client_sockfd);
		return LOOP_OPERATION_NONE;
	}

	if (conn->protocol == FHTTPD_PROTOCOL_UNKNOWN)
	{
		fhttpd_wclog_error ("No known protocol for connection #%lu", conn->id);
		fhttpd_server_free_connection (server, conn);
		return LOOP_OPERATION_NONE;
	}

	if (conn->request_count > conn->response_count)
	{
		fhttpd_wclog_debug ("Connection #%lu: Some requests are being processed", conn->id);
		conn->responses = realloc (conn->responses, sizeof (struct fhttpd_response) * conn->request_count);

		if (!conn->responses)
		{
			fhttpd_wclog_error ("Memory allocation failed for responses in connection #%lu", conn->id);
			fhttpd_server_free_connection (server, conn);
			return LOOP_OPERATION_NONE;
		}

		size_t prev_response_count = conn->response_count;
		conn->response_count = conn->request_count;

		if (conn->response_count > 1 && conn->protocol == FHTTPD_PROTOCOL_HTTP1x)
		{
			fhttpd_wclog_error ("Connection #%lu: Multiple requests in HTTP/1.x protocol are not supported", conn->id);
			fhttpd_server_free_connection (server, conn);
			return LOOP_OPERATION_NONE;
		}

		memset (&conn->responses[prev_response_count], 0,
				sizeof (struct fhttpd_response) * (conn->response_count - prev_response_count));

		for (size_t i = prev_response_count; i < conn->request_count; i++)
		{
			struct fhttpd_response *response = &conn->responses[i];

			if (!fhttpd_process_request (server, conn, i, response))
			{
				fhttpd_wclog_debug ("Connection #%lu: Failed to process request", conn->id);

				if (fhttpd_connection_defer_error_response (conn, i + 1, FHTTPD_STATUS_INTERNAL_SERVER_ERROR))
					fhttpd_connection_send_response (conn, i + 1, NULL);

				fhttpd_server_free_connection (server, conn);
				return LOOP_OPERATION_NONE;
			}
		}
	}

	if (conn->response_count == 0)
	{
		fhttpd_wclog_debug ("Connection #%lu: No responses to send", conn->id);
		fhttpd_server_free_connection (server, conn);
		return LOOP_OPERATION_NONE;
	}

	if (conn->protocol == FHTTPD_PROTOCOL_HTTP1x)
	{
		memset (&conn->proto.http1.http1_res_ctx, 0, sizeof (conn->proto.http1.http1_res_ctx));
		conn->proto.http1.http1_res_ctx.fd = -1;
	}

	bool all_responses_sent = true;

	for (size_t i = 0; i < conn->response_count; i++)
	{
		struct fhttpd_response *response = &conn->responses[i];

		if (response->sent)
			continue;

		if (!response->ready)
		{
			fhttpd_wclog_debug ("Connection #%lu: Response #%zu is not ready to be sent", conn->id, i);
			all_responses_sent = false;
			continue;
		}

		errno = 0;

		if (!fhttpd_connection_send_response (conn, i, response))
		{
			fhttpd_wclog_debug ("Connection #%lu: Failed to send response", conn->id);
			fhttpd_server_free_connection (server, conn);
			return LOOP_OPERATION_NONE;
		}

		if (would_block ())
		{
			fhttpd_wclog_debug ("Connection #%lu: Client cannot accept data right now, waiting for next event",
								conn->id);
			return LOOP_OPERATION_NONE;
		}

		response->sent = true;
	}

	if (!all_responses_sent)
	{
		fhttpd_wclog_debug ("Connection #%lu: Not all responses were sent, waiting for next event", conn->id);
		return LOOP_OPERATION_NONE;
	}

	fhttpd_wclog_info ("Connection #%lu: Response sent successfully", conn->id);
	fhttpd_server_free_connection (server, conn);
	return LOOP_OPERATION_NONE;
}

static loop_op_t
fhttpd_server_on_hup (struct fhttpd_server *server, fd_t client_sockfd)
{
	struct fhttpd_connection *conn = itable_get (server->connections, client_sockfd);

	if (!conn)
	{
		fhttpd_wclog_error ("Client hangup received, no connection found for socket: %d", client_sockfd);
		epoll_ctl (server->epoll_fd, EPOLL_CTL_DEL, client_sockfd, NULL);
		close (client_sockfd);
		return LOOP_OPERATION_NONE;
	}

	fhttpd_wclog_info ("Client hangup received in connection: %lu", conn->id);
	fhttpd_server_free_connection (server, conn);

	return LOOP_OPERATION_NONE;
}

static bool
fhttpd_create_timerfd (struct fhttpd_server *server, time_t interval_sec)
{
	int timerfd = timerfd_create (CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

	if (timerfd < 0)
		return false;

	struct itimerspec timer_spec = { 0 };

	timer_spec.it_value.tv_sec = interval_sec;
	timer_spec.it_value.tv_nsec = 0;
	timer_spec.it_interval.tv_sec = interval_sec;
	timer_spec.it_interval.tv_nsec = 0;

	if (timerfd_settime (timerfd, 0, &timer_spec, NULL) < 0)
	{
		close (timerfd);
		return false;
	}

	if (!fhttpd_epoll_ctl_add (server, timerfd, EPOLLIN))
	{
		close (timerfd);
		return false;
	}

	server->timer_fd = timerfd;
	return true;
}

static bool
fhttpd_server_check_connections (struct fhttpd_server *server)
{
	if (!server || !server->connections)
		return false;

	struct itable_entry *entry = server->connections->head;
	size_t count = 0;

	while (entry)
	{
		struct fhttpd_connection *conn = entry->data;
		uint64_t current_time = get_current_timestamp ();

		if (conn)
		{
			const struct fhttpd_config_host *host_config
				= strtable_get (server->host_config_table, conn->full_hostname);
			const struct fhttpd_config *config = host_config ? host_config->host_config : server->config;

			const uint32_t recv_timeout = config->sec_recv_timeout;
			const uint32_t send_timeout = config->sec_send_timeout;
			const uint32_t header_timeout = config->sec_header_timeout;
			const uint32_t body_timeout = config->sec_body_timeout;

			fhttpd_wclog_debug ("Checking connection #%lu", conn->id);

			bool is_recv_timeout = conn->last_recv_timestamp + recv_timeout < current_time;
			bool is_send_timeout = conn->last_send_timestamp + send_timeout < current_time;

			if (conn->protocol == FHTTPD_PROTOCOL_HTTP1x)
			{
				fhttpd_wclog_debug ("Connection #%lu: HTTP/1.x protocol", conn->id);

				enum http1_parser_state state = conn->proto.http1.http1_req_ctx.state;

				if (state == HTTP1_STATE_RECV)
					state = conn->proto.http1.http1_req_ctx.prev_state;

				fhttpd_wclog_debug ("Connection #%lu: HTTP/1.x parser state: %d", conn->id, state);
				fhttpd_wclog_debug ("Connection #%lu: Last recv timestamp: %lu, Last send timestamp: %lu", conn->id,
									conn->last_recv_timestamp, conn->last_send_timestamp);
				fhttpd_wclog_debug ("Connection #%lu: Recv interval: %lu, Send interval: %lu", conn->id,
									current_time - conn->last_recv_timestamp, current_time - conn->last_send_timestamp);
				fhttpd_wclog_debug ("Connection #%lu: Total time elapsed since last valid request: %lu", conn->id,
									current_time - conn->last_request_timestamp);

				if (state != HTTP1_STATE_DONE && state != HTTP1_STATE_BODY
					&& conn->last_request_timestamp + header_timeout < current_time)
				{
					fhttpd_wclog_info ("Connection #%lu timed out recv: Client did not finish sending headers in time",
									   conn->id);
					goto fhttpd_server_check_connections_close_next;
				}

				if (state == HTTP1_STATE_BODY && conn->last_request_timestamp + body_timeout < current_time)
				{
					fhttpd_wclog_info ("Connection #%lu timed out recv: Client did not finish sending body in time",
									   conn->id);
					goto fhttpd_server_check_connections_close_next;
				}
			}

			if (is_recv_timeout && is_send_timeout)
			{
				fhttpd_wclog_info ("Connection #%lu timed out due to no activity", conn->id);
				goto fhttpd_server_check_connections_close_next;
			}

			if ((conn->mode != FHTTPD_CONNECTION_MODE_WRITE || conn->protocol == FHTTPD_PROTOCOL_UNKNOWN)
				&& is_recv_timeout)
			{
				fhttpd_wclog_info ("Connection #%lu timed out on recv", conn->id);
				goto fhttpd_server_check_connections_close_next;
			}

			if (conn->mode != FHTTPD_CONNECTION_MODE_READ && is_send_timeout)
			{
				fhttpd_wclog_info ("Connection #%lu timed out on recv", conn->id);
				goto fhttpd_server_check_connections_close_next;
			}
		}

		entry = entry->next;
		continue;

	fhttpd_server_check_connections_close_next:
		entry = entry->next;
		fhttpd_server_free_connection (server, conn);
		count++;
	}

	fhttpd_wclog_debug ("Closed %zu connections due to timeouts", count);
	return true;
}

__noreturn void
fhttpd_server_loop (struct fhttpd_server *server)
{
	struct epoll_event events[MAX_EVENTS];

	if (!fhttpd_create_timerfd (server, 5))
	{
		fhttpd_wclog_error ("Failed to create timerfd: %s", strerror (errno));
		exit (EXIT_FAILURE);
	}

	fd_t timerfd = server->timer_fd;

	while (true)
	{
		int nfds = epoll_wait (server->epoll_fd, events, MAX_EVENTS, -1);

		if (nfds < 0)
		{
			fhttpd_wclog_error ("epoll_wait() returned %d", nfds);
			exit (EXIT_FAILURE);
		}

		for (int i = 0; i < nfds; i++)
		{
			if (events[i].data.fd == timerfd)
			{
				uint64_t expirations = 0;
				ssize_t s = read (timerfd, &expirations, sizeof (expirations));

				if (s < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
				{
					fhttpd_wclog_error ("Failed to read from timerfd: %s", strerror (errno));
					continue;
				}

				if (expirations > 0 && s == sizeof (expirations))
				{
					if (!fhttpd_server_check_connections (server))
					{
						fhttpd_wclog_error ("Failed to check connections: %s", strerror (errno));
						continue;
					}
				}

				continue;
			}

			bool is_listen_fd = false;

			for (size_t j = 0; j < server->listen_fd_count; j++)
			{
				if (server->listen_fds[j] == events[i].data.fd)
				{
					if (!fhttpd_server_accept (server, j))
					{
						if (errno == EAGAIN)
							fhttpd_wclog_error ("Max connections reached, cannot accept any new connection");
						else
							fhttpd_wclog_error ("Error accepting new connection: %s", strerror (errno));
					}

					is_listen_fd = true;
					break;
				}
			}

			if (is_listen_fd)
				continue;

			uint32_t ev = events[i].events;

			if (ev & EPOLLIN)
			{
				loop_op_t op = fhttpd_server_on_read_ready (server, events[i].data.fd);
				LOOP_OPERATION (op);
			}
			else if (ev & EPOLLOUT)
			{
				loop_op_t op = fhttpd_server_on_write_ready (server, events[i].data.fd);
				LOOP_OPERATION (op);
			}

			if (ev & EPOLLHUP)
			{
				loop_op_t op = fhttpd_server_on_hup (server, events[i].data.fd);
				LOOP_OPERATION (op);
			}
		}
	}
}
