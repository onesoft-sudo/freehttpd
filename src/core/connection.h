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

#ifndef FHTTPD_CONNECTION_H
#define FHTTPD_CONNECTION_H

#include <arpa/inet.h>
#include <stdint.h>

#include "http/http1.h"
#include "http/protocol.h"
#include "types.h"

enum fhttpd_connection_mode
{
	FHTTPD_CONNECTION_MODE_READ,
	FHTTPD_CONNECTION_MODE_WRITE,
	FHTTPD_CONNECTION_MODE_FULL_DUPLEX,
};

struct fhttpd_connection
{
	uint64_t id;

	fd_t client_sockfd;
	protocol_t protocol;
	char exact_protocol[4];

	uint64_t last_recv_timestamp;
	uint64_t last_send_timestamp;
	uint64_t last_request_timestamp;
	uint64_t created_at;

	union
	{
		char protobuf[H2_PREFACE_SIZE];
	} buffers;

	size_t buffer_size;

	union
	{
		struct
		{
			struct http1_parser_ctx http1_req_ctx;
			struct http1_response_ctx http1_res_ctx;
		} http1;
	} parsers;

	struct fhttpd_request *requests;
	size_t request_count;

	struct fhttpd_response *responses;
	size_t response_count;

	uint16_t port;
	const char *hostname;
	size_t hostname_len;
	const char *full_hostname;
	size_t full_hostname_len;

	enum fhttpd_connection_mode mode;

	struct fhttpd_config_host *config;
};

struct fhttpd_connection *fhttpd_connection_create (uint64_t id, fd_t client_sockfd);
void fhttpd_connection_close (struct fhttpd_connection *conn);
ssize_t fhttpd_connection_recv (struct fhttpd_connection *conn, void *buf, size_t size, int flags);
bool fhttpd_connection_detect_protocol (struct fhttpd_connection *conn);
ssize_t fhttpd_connection_send (struct fhttpd_connection *conn, const void *buf, size_t size, int flags);
ssize_t fhttpd_connection_sendfile (struct fhttpd_connection *conn, int src_fd, off_t *offset, size_t count);
bool fhttpd_connection_defer_response (struct fhttpd_connection *conn, size_t response_index,
									   const struct fhttpd_response *response);
bool fhttpd_connection_defer_error_response (struct fhttpd_connection *conn, size_t response_index,
											 enum fhttpd_status code);
bool fhttpd_connection_send_response (struct fhttpd_connection *conn, size_t response_index,
									  const struct fhttpd_response *response);

#endif /* FHTTPD_CONNECTION_H */
