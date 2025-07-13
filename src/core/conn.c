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

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FH_LOG_MODULE_NAME "conn"

#include "conn.h"
#include "http/protocol.h"
#include "log/log.h"

#ifdef HAVE_RESOURCES
	#include "resources.h"
#endif

static object_id_t next_conn_id = 0;

struct fh_conn *
fh_conn_create (fd_t client_sockfd, const struct sockaddr_in *client_addr, const struct sockaddr_in *server_addr)
{
	pool_t *pool = fh_pool_create (0);

	if (!pool)
		return NULL;

	struct fh_conn *conn = fh_pool_alloc (pool, sizeof (*conn) + sizeof (*client_addr) + sizeof (*conn->stream)
													+ sizeof (*conn->requests) + sizeof (*conn->extra));

	if (!conn)
		return NULL;

	conn->id = next_conn_id++;
	conn->client_addr = (struct sockaddr_in *) (conn + 1);
	conn->stream = (struct fh_stream *) (conn->client_addr + 1);
	conn->client_sockfd = client_sockfd;
	conn->pool = pool;
	conn->server_addr = server_addr;
	conn->req_ctx = NULL;
	conn->requests = (struct fh_requests *) (conn->stream + 1);
	conn->extra = (struct fh_conn_extra *) (conn->requests + 1);
	
	memset (conn->requests, 0, sizeof (*conn->requests) + sizeof (*conn->extra));
	return conn;
}

void
fh_conn_destroy (struct fh_conn *conn)
{
	fh_pr_debug ("Connection #%lu will now be deallocated", conn->id);

	struct fh_request *r = conn->requests->head;

	while (r)
	{
		struct fh_pool *r_pool = r->pool;
		struct fh_request *r_next = r->next;

		if (r_pool)
			fh_pool_destroy (r_pool);

		r = r_next;
	}

	pool_t *pool = conn->pool;
	close (conn->client_sockfd);
	fh_pool_destroy (pool);
}

void
fh_conn_push_request (struct fh_requests *requests, struct fh_request *request)
{
	request->next = NULL;

	if (!requests->head)
	{
		requests->head = requests->tail = request;
		requests->count = 1;
	}
	else
	{
		requests->tail->next = request;
		requests->tail = request;
		requests->count++;
	}
}

struct fh_request *
fh_conn_pop_request (struct fh_requests *requests)
{
	if (!requests->head)
		return NULL;

	struct fh_request *request = requests->head;

	if (requests->count == 1)
		requests->tail = requests->head = NULL;
	else
		requests->head = requests->head->next;

	requests->count--;
	return request;
}

bool
fh_conn_send_err_response (struct fh_conn *conn, enum fh_status code)
{
	size_t status_text_len = 0, description_len = 0;
	const char *status_text = fh_get_status_text (code, &status_text_len);
	const char *description = fh_get_status_description (code, &description_len);
	const size_t host_len = conn->extra->host_len < INT32_MAX ? conn->extra->host_len : INT32_MAX;
	const size_t port_len = conn->extra->port < 10		? 1
							: conn->extra->port < 100	? 2
							: conn->extra->port < 1000	? 3
							: conn->extra->port < 10000 ? 4
														: 5;
	const size_t response_len = resource_error_html_len - (2 * 7) - 2 + (2 * 3) + (2 * status_text_len) + description_len
								+ host_len + port_len;
	int rc;

	rc = dprintf (
		conn->client_sockfd,
		"HTTP/1.1 %d %s\r\nServer: freehttpd\r\nContent-Length: %zu\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n",
		code, status_text, response_len);

	if (rc < 0)
		return false;

	rc = dprintf (conn->client_sockfd, resource_error_html, code, status_text, code, status_text, description,
				  (int) host_len, conn->extra->host, conn->extra->port);

	return rc >= 0;
}
