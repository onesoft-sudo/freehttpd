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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define FHTTPD_LOG_MODULE_NAME "conn"

#include "conn.h"
#include "http/http1.h"
#include "log/log.h"
#include "utils/utils.h"

#ifdef HAVE_RESOURCES
	#include "resources.h"
#endif

#define FHTTPD_CONN_MM_POOL_DEFAULT_SIZE 1

bool
fh_conn_init (struct fh_conn *conn, uint64_t id, fd_t client_sockfd)
{
	conn->pool = fh_pool_create (FHTTPD_CONN_MM_POOL_DEFAULT_SIZE);

	if (!conn->pool)
		return false;

	conn->id = id;
	conn->protocol = FHTTPD_PROTOCOL_UNKNOWN;
	conn->is_used = true;
	conn->client_sockfd = client_sockfd;
	conn->last_recv_timestamp = get_current_timestamp ();
	conn->last_send_timestamp = conn->last_recv_timestamp;
	conn->created_at = conn->last_recv_timestamp;
	conn->last_request_timestamp = conn->last_recv_timestamp;
	conn->is_heap = false;
	
	return true;
}

struct fh_conn *
fh_conn_create (uint64_t id, fd_t client_sockfd)
{
	struct fh_conn *conn = calloc (1, sizeof (*conn));

	if (!fh_conn_init (conn, id, client_sockfd))
	{
		free (conn);
		return NULL;
	}

	conn->is_heap = true;
	return conn;
}

void
fh_conn_close (struct fh_conn *conn)
{
	close (conn->client_sockfd);

	for (size_t i = 0; i < conn->request_count; i++)
		fhttpd_request_free (&conn->requests[i]);

	free (conn->requests);

	if (conn->protocol == FHTTPD_PROTOCOL_HTTP_1_1 || conn->protocol == FHTTPD_PROTOCOL_HTTP_1_0)
	{
		if (conn->ioh.http1.http1_req_ctx)
			http1_parser_ctx_free (conn->ioh.http1.http1_req_ctx);

		if (conn->ioh.http1.http1_res_ctx)
			http1_response_ctx_free (conn->ioh.http1.http1_res_ctx);
	}

	fh_pool_destroy (conn->pool);

	if (conn->is_heap)
		free (conn);
}

ssize_t
fh_conn_recv (struct fh_conn *conn, void *buf, size_t size, int flags)
{
	errno = 0;

	ssize_t bytes_read = recv (conn->client_sockfd, buf, size, flags);

	if (bytes_read < 0)
		return bytes_read;

	int err = errno;
	conn->last_recv_timestamp = get_current_timestamp ();
	errno = err;

	return bytes_read;
}

bool
fh_conn_detect_protocol (struct fh_conn *conn)
{
	while (conn->proto_detect_buffer_size < H2_PREFACE_SIZE)
	{
		ssize_t bytes_read = fh_conn_recv (conn, conn->proto_detect_buffer + conn->proto_detect_buffer_size,
										   H2_PREFACE_SIZE - conn->proto_detect_buffer_size, 0);

		if (bytes_read < 0)
		{
			return would_block ();
		}
		else if (bytes_read == 0)
		{
			conn->protocol = FHTTPD_PROTOCOL_HTTP_1_1;
			return true;
		}

		conn->proto_detect_buffer_size += bytes_read;
	}

	if (memcmp (conn->proto_detect_buffer, H2_PREFACE, H2_PREFACE_SIZE) == 0)
		conn->protocol = FHTTPD_PROTOCOL_H2;
	else
		conn->protocol = FHTTPD_PROTOCOL_HTTP_1_1;

	return true;
}

ssize_t
fh_conn_send (struct fh_conn *conn, const void *buf, size_t size, int flags)
{
	errno = 0;

	ssize_t bytes_sent = send (conn->client_sockfd, buf, size, flags);

	if (bytes_sent <= 0)
		return bytes_sent;

	int err = errno;
	conn->last_send_timestamp = get_current_timestamp ();
	errno = err;

	return bytes_sent;
}

ssize_t
fh_conn_sendfile (struct fh_conn *conn, int src_fd, off_t *offset, size_t count)
{
	errno = 0;

	ssize_t bytes_sent = sendfile (conn->client_sockfd, src_fd, offset, count);

	if (bytes_sent <= 0)
		return bytes_sent;

	int err = errno;
	conn->last_send_timestamp = get_current_timestamp ();
	errno = err;

	return bytes_sent;
}
