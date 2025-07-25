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

#ifndef FH_HTTP1_REQUEST_H
#define FH_HTTP1_REQUEST_H

#include <stdbool.h>

#include "core/conn.h"
#include "core/stream.h"
#include "core/server.h"
#include "mm/pool.h"
#include "protocol.h"
#include "http1.h"

#define HTTP1_METHOD_MAX_LEN 16
#define HTTP1_VERSION_MAX_LEN 8
#define HTTP1_URI_MAX_LEN   4096
#define HTTP1_HEADER_NAME_MAX_LEN 128
#define HTTP1_HEADER_VALUE_MAX_LEN 256
#define HTTP1_HEADER_COUNT_MAX 128
#define HTTP1_HOST_MAX_LEN 256

enum http1_req_state
{
	H1_REQ_STATE_METHOD,
	H1_REQ_STATE_URI,
	H1_REQ_STATE_VERSION,
	H1_REQ_STATE_HEADER_NAME,
	H1_REQ_STATE_HEADER_VALUE,
	H1_REQ_STATE_BODY,
	H1_REQ_STATE_RECV,
	H1_REQ_STATE_ERROR,
	H1_REQ_STATE_DONE
};

struct fh_http1_req_result
{

	const char *uri;
	size_t uri_len;

	const char *version;
	size_t version_len;

	enum fh_method method;
	struct fh_headers headers;

	struct fh_link *body_start;
};

struct fh_http1_req_ctx
{
	struct fh_server *server;
	struct fh_stream *stream;
	struct fh_http1_cursor cur;
	struct fh_http1_cursor arg_cur;
	struct fh_request request;
	const char *current_header_name;
	size_t current_header_name_len;
	size_t total_consumed;
	size_t current_consumed;
	uint16_t suggested_code;
	uint8_t state : 4;
	uint8_t prev_state : 4;
	bool is_streaming_body;
};

struct fh_http1_req_ctx *fh_http1_ctx_create (struct fh_server *server, struct fh_conn *conn, struct fh_stream *stream);
bool fh_http1_parse (struct fh_http1_req_ctx *ctx, struct fh_conn *conn);

#endif /* FH_HTTP1_REQUEST_H */
