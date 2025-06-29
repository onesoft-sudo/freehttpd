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

struct fh_conn *
fh_conn_create (uint64_t id, fd_t client_sockfd)
{
	struct fh_conn *conn = calloc (1, sizeof (struct fh_conn));

	if (!conn)
		return NULL;

	conn->pool = fh_pool_create (4096);

	if (!conn->pool)
	{
		free (conn);
		return NULL;
	}

	conn->id = id;
	conn->client_sockfd = client_sockfd;
	conn->last_recv_timestamp = get_current_timestamp ();
	conn->last_send_timestamp = conn->last_recv_timestamp;
	conn->created_at = conn->last_recv_timestamp;
	conn->last_request_timestamp = conn->last_recv_timestamp;

	return conn;
}

void
fh_conn_close (struct fh_conn *conn)
{
	if (!conn)
		return;

	fh_pool_destroy (conn->pool);

	for (size_t i = 0; i < conn->request_count; i++)
		fhttpd_request_free (&conn->requests[i]);

	free (conn->requests);

	if (conn->protocol == FHTTPD_PROTOCOL_HTTP_1_1 || conn->protocol == FHTTPD_PROTOCOL_HTTP_1_0)
	{
		http1_parser_ctx_free (&conn->ioh.http1.http1_req_ctx);
		http1_response_ctx_free (&conn->ioh.http1.http1_res_ctx);
	}

	close (conn->client_sockfd);
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

bool
fh_conn_defer_response (struct fh_conn *conn, size_t response_index,
								  const struct fhttpd_response *response)
{
	if (conn->deferred_response_count <= response_index)
	{
		struct fhttpd_response *responses
			= realloc (conn->deferred_responses, sizeof (struct fhttpd_response) * (response_index + 1));

		if (!responses)
			return false;

		conn->deferred_responses = responses;
		conn->deferred_response_count = response_index + 1;
	}

	struct fhttpd_response *new_response = &conn->deferred_responses[response_index];

	memcpy (new_response, response, sizeof (struct fhttpd_response));
	new_response->is_deferred = true;

	if (new_response->headers.count == 0)
	{
		struct fhttpd_header *list = malloc (sizeof (struct fhttpd_header) * 2);

		if (!list)
			return false;

		new_response->headers.list = list;

		fhttpd_header_add_noalloc (&new_response->headers, 0, "Server", "freehttpd", 6, 9);
		fhttpd_header_add_noalloc (&new_response->headers, 1, "Connection", "close", 10, 5);
	}

	new_response->ready = true;
	return true;
}

bool
fh_conn_defer_error_response (struct fh_conn *conn, size_t response_index, enum fhttpd_status code)
{
	struct fhttpd_response response = {
		.status = code,
		.headers = { .list = NULL, .count = 0 },
		.body = NULL,
		.body_len = 0,
		.use_builtin_error_response = true,
	};

	return fh_conn_defer_response (conn, response_index, &response);
}

bool
fh_conn_send_response (struct fh_conn *conn, size_t response_index,
								 const struct fhttpd_response *response)
{
	if (!response)
	{
		if (response_index >= conn->deferred_response_count)
			return false;

		response = &conn->deferred_responses[response_index];
	}

	struct http1_response_ctx *ctx = &conn->ioh.http1.http1_res_ctx;

	while (true)
	{
		if (ctx->sending_rn)
		{
			if (ctx->sent_bytes > 2)
			{
				ctx->sending_file = false;
				return false;
			}

			ctx->offset = 0;

			memcpy (ctx->buffer, ctx->sent_bytes == 1 ? "\n" : "\r\n", 2 - ctx->sent_bytes);
			ctx->buffer_len = 2 - ctx->sent_bytes;

			ssize_t bytes_sent
				= fh_conn_send (conn, ctx->buffer, ctx->buffer_len, MSG_DONTWAIT | MSG_NOSIGNAL);

			if (bytes_sent < 0 && would_block ())
			{
				return true;
			}

			if (bytes_sent <= 0)
			{
				ctx->sending_file = false;
				return false;
			}

			ctx->sent_bytes += bytes_sent;

			if (ctx->sent_bytes >= 2)
			{
				ctx->sending_rn = false;
				ctx->sending_file = true;
				ctx->offset = 0;
				ctx->sent_bytes = 0;

				if (ctx->fd < 1)
					return ctx->eos;
			}

			continue;
		}

		if (ctx->sending_file)
		{
			ssize_t bytes_sent
				= fh_conn_sendfile (conn, ctx->fd, &ctx->offset, response->body_len - ctx->sent_bytes);

			if (bytes_sent < 0 && would_block ())
			{
				return true;
			}

			if (bytes_sent <= 0)
			{
				ctx->sending_file = false;
				return false;
			}

			ctx->sent_bytes += bytes_sent;
			fhttpd_wclog_debug ("Connection #%lu: Sent %zu bytes from file", conn->id, bytes_sent);

			if (ctx->sent_bytes >= response->body_len)
			{
				ctx->sending_file = false;
				ctx->eos = true;
				break;
			}

			continue;
		}

		if (ctx->buffer_len <= ctx->sent_bytes)
		{
			ctx->buffer_len = 0;
			ctx->sent_bytes = 0;

			if (!ctx->drain_first && !http1_response_buffer (ctx, conn, response))
			{
				if (!ctx->eos)
					return false;

				ctx->fd = response->fd;
				ctx->sending_rn = true;
				ctx->sent_bytes = 0;
				continue;
			}
		}

		ssize_t bytes_sent = fh_conn_send (conn, ctx->buffer + ctx->sent_bytes,
													 ctx->buffer_len - ctx->sent_bytes, MSG_DONTWAIT | MSG_NOSIGNAL);

		if (bytes_sent < 0 && would_block ())
		{
			return true;
		}

		if (bytes_sent == 0)
			return false;

		ctx->sent_bytes += bytes_sent;
		fhttpd_wclog_debug ("Connection #%lu: Sent %zu bytes", conn->id, bytes_sent);
	}

	return true;
}
