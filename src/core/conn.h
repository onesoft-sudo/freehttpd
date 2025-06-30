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

#ifndef FHTTPD_CONN_H
#define FHTTPD_CONN_H

#include <arpa/inet.h>
#include <stdint.h>

#include "http/http1.h"
#include "http/protocol.h"
#include "mm/pool.h"
#include "types.h"

struct fh_io_h_http1
{
	struct http1_parser_ctx *http1_req_ctx;
	struct http1_response_ctx *http1_res_ctx;
};

union fh_io_handlers
{
	struct fh_io_h_http1 http1;
};

struct fh_conn
{
	uint64_t id;
	struct fh_pool *pool;
	fd_t client_sockfd;
	protocol_t protocol;
	uint64_t last_recv_timestamp;
	uint64_t last_send_timestamp;
	uint64_t last_request_timestamp;
	uint64_t created_at;
	uint8_t proto_detect_buffer[H2_PREFACE_SIZE];
	size_t proto_detect_buffer_size;
	union fh_io_handlers io_h;
	const struct fhttpd_config_host *config;
	struct fhttpd_request *requests;
	struct fhttpd_response *deferred_responses;
	size_t request_count;
	size_t request_cap;
	size_t deferred_response_count;
	struct fh_conn *next;
	const char *hostname;
	const char *full_hostname;
	size_t hostname_len;
	size_t full_hostname_len;
	uint16_t port;
	bool is_used : 1;
	bool is_heap : 1;
};

bool fh_conn_init (struct fh_conn *conn, uint64_t id, fd_t client_sockfd);
struct fh_conn *fh_conn_create (uint64_t id, fd_t client_sockfd);
void fh_conn_close (struct fh_conn *conn);
ssize_t fh_conn_recv (struct fh_conn *conn, void *buf, size_t size, int flags);
bool fh_conn_detect_protocol (struct fh_conn *conn);
ssize_t fh_conn_send (struct fh_conn *conn, const void *buf, size_t size, int flags);
ssize_t fh_conn_sendfile (struct fh_conn *conn, int src_fd, off_t *offset, size_t count);

#endif /* FHTTPD_CONN_H */
