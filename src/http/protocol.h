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

#ifndef FHTTPD_PROTOCOL_H
#define FHTTPD_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "types.h"

#define H2_PREFACE "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
#define H2_PREFACE_SIZE (sizeof (H2_PREFACE) - 1)

enum fhttpd_protocol
{
	FHTTPD_PROTOCOL_UNKNOWN,
	FHTTPD_PROTOCOL_HTTP_1X,
	FHTTPD_PROTOCOL_H2
};

typedef enum fhttpd_protocol protocol_t;

struct fhttpd_connection; /* Forward declaration */

enum fhttpd_method
{
	FHTTPD_METHOD_GET,
	FHTTPD_METHOD_POST,
	FHTTPD_METHOD_PUT,
	FHTTPD_METHOD_DELETE,
	FHTTPD_METHOD_HEAD,
	FHTTPD_METHOD_OPTIONS,
	FHTTPD_METHOD_PATCH,
	FHTTPD_METHOD_CONNECT,
	FHTTPD_METHOD_TRACE
};

enum fhttpd_status
{
	FHTTPD_STATUS_OK = 200,
	FHTTPD_STATUS_CREATED = 201,
	FHTTPD_STATUS_ACCEPTED = 202,
	FHTTPD_STATUS_NO_CONTENT = 204,
	FHTTPD_STATUS_BAD_REQUEST = 400,
	FHTTPD_STATUS_UNAUTHORIZED = 401,
	FHTTPD_STATUS_FORBIDDEN = 403,
	FHTTPD_STATUS_NOT_FOUND = 404,
	FHTTPD_STATUS_METHOD_NOT_ALLOWED = 405,
	FHTTPD_STATUS_REQUEST_URI_TOO_LONG = 414,
	FHTTPD_STATUS_INTERNAL_SERVER_ERROR = 500,
	FHTTPD_STATUS_NOT_IMPLEMENTED = 501,
	FHTTPD_STATUS_SERVICE_UNAVAILABLE = 503,
};

struct fhttpd_header
{
	char *name;
	char *value;
	size_t name_length;
	size_t value_length;
};

struct fhttpd_headers
{
	struct fhttpd_header *list;
	size_t count;
};

struct fhttpd_request
{
	struct fhttpd_connection *conn;
	protocol_t protocol;
	enum fhttpd_method method;
	char *host;
	size_t host_len;
	char *full_host;
	size_t full_host_len;
	uint16_t host_port;
	char *uri;
	size_t uri_len;
	char *path;
	size_t path_len;
	char *qs;
	size_t qs_len;
	struct fhttpd_headers headers;
	uint8_t *body;
	uint64_t body_len;
};

struct fhttpd_response
{
	enum fhttpd_status status;
	struct fhttpd_headers headers;
	uint8_t *body;
	uint64_t body_len;
	fd_t fd;
	bool set_content_length;
	bool is_deferred;
	bool use_builtin_error_response;
	bool sent, ready;
};

const char *fhttpd_protocol_to_string (enum fhttpd_protocol protocol);
enum fhttpd_protocol fhttpd_string_to_protocol (const char *protocol_str);

const char *fhttpd_method_to_string (enum fhttpd_method method);

bool fhttpd_validate_header_name (const char *name, size_t len);

const char *fhttpd_get_status_text (enum fhttpd_status code);
const char *fhttpd_get_status_description (enum fhttpd_status code);

bool fhttpd_header_add (struct fhttpd_headers *headers, const char *name, const char *value, size_t name_length,
						size_t value_length);
bool fhttpd_header_add_noalloc (struct fhttpd_headers *headers, size_t index, const char *name, const char *value,
								size_t name_length, size_t value_length);
bool fhttpd_header_add_noalloc_printf (struct fhttpd_headers *headers, size_t index, const char *name,
									   size_t name_length, const char *value_format, ...);
bool fhttpd_header_add_printf (struct fhttpd_headers *headers, const char *name, size_t name_length,
							   const char *value_format, ...);

void fhttpd_request_free (struct fhttpd_request *request, bool inner_only);

#endif /* FHTTPD_PROTOCOL_H */
