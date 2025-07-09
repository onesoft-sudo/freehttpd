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

#ifndef FH_PROTOCOL_H
#define FH_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "types.h"

#define H2_PREFACE "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
#define H2_PREFACE_SIZE (sizeof (H2_PREFACE) - 1)

enum fh_protocol
{
	FH_PROTOCOL_UNKNOWN,
	FH_PROTOCOL_HTTP_1_0,
	FH_PROTOCOL_HTTP_1_1,
	FH_PROTOCOL_H2
};

typedef enum fh_protocol protocol_t;

struct fh_conn; /* Forward declaration */

enum fh_method
{
	FH_METHOD_GET,
	FH_METHOD_POST,
	FH_METHOD_PUT,
	FH_METHOD_DELETE,
	FH_METHOD_HEAD,
	FH_METHOD_OPTIONS,
	FH_METHOD_PATCH,
	FH_METHOD_CONNECT,
	FH_METHOD_TRACE
};

enum fh_status
{
	FH_STATUS_OK = 200,
	FH_STATUS_CREATED = 201,
	FH_STATUS_ACCEPTED = 202,
	FH_STATUS_NO_CONTENT = 204,
	FH_STATUS_BAD_REQUEST = 400,
	FH_STATUS_UNAUTHORIZED = 401,
	FH_STATUS_FORBIDDEN = 403,
	FH_STATUS_NOT_FOUND = 404,
	FH_STATUS_METHOD_NOT_ALLOWED = 405,
	FH_STATUS_REQUEST_URI_TOO_LONG = 414,
	FH_STATUS_INTERNAL_SERVER_ERROR = 500,
	FH_STATUS_NOT_IMPLEMENTED = 501,
	FH_STATUS_SERVICE_UNAVAILABLE = 503,
};

enum fh_encoding
{
	FH_ENCODING_PLAIN,
	FH_ENCODING_CHUNKED
};

struct fh_header
{
	const char *name;
	const char *value;
	size_t name_len;
	size_t value_len;
	struct fh_header *next;
};

struct fh_headers
{
	struct fh_header *head;
	struct fh_header *tail;
	size_t count;
};

struct fh_request
{
	const char *uri;
	size_t uri_len;	
	struct fh_headers headers;
	struct fh_link *body_start;
	
	uint64_t content_length;
	uint8_t transfer_encoding : 4;
	uint8_t protocol : 4;
	uint8_t method : 4;
};

const char *fh_protocol_to_string (enum fh_protocol protocol);
enum fh_protocol fh_string_to_protocol (const char *protocol_str);

const char *fh_method_to_string (enum fh_method method);

bool fh_validate_header_name (const char *name, size_t len);

const char *fh_get_status_text (enum fh_status code);
const char *fh_get_status_description (enum fh_status code);

void fh_headers_init (struct fh_headers *headers);
struct fh_header *fh_header_add (pool_t *pool, struct fh_headers *headers, const char *name, size_t name_len, const char *value, size_t value_len);
struct fh_header *fh_header_addf (pool_t *pool, struct fh_headers *headers, const char *name, size_t name_len, const char *value_format, ...);

#endif /* FH_PROTOCOL_H */
