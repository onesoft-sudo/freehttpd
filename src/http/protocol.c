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

#define _GNU_SOURCE

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "compat.h"
#include "core/conn.h"
#include "core/stream.h"
#include "mm/pool.h"
#include "protocol.h"

const char *
fh_protocol_to_string (enum fh_protocol protocol)
{
	switch (protocol)
	{
		case FH_PROTOCOL_HTTP_1_0:
			return "HTTP/1.0";
		case FH_PROTOCOL_HTTP_1_1:
			return "HTTP/1.1";
		case FH_PROTOCOL_H2:
			return "h2";
		default:
			return "Unknown Protocol";
	}
}

enum fh_protocol
fh_string_to_protocol (const char *protocol_str)
{
	if (strcmp (protocol_str, "HTTP/1.0") == 0)
		return FH_PROTOCOL_HTTP_1_0;
	if (strcmp (protocol_str, "HTTP/1.1") == 0)
		return FH_PROTOCOL_HTTP_1_1;
	else if (strcmp (protocol_str, "HTTP/2.0") == 0 || strcmp (protocol_str, "h2") == 0
			 || strcmp (protocol_str, "h2c") == 0)
		return FH_PROTOCOL_H2;
	else
		return -1;
}

const char *
fh_method_to_string (enum fh_method method)
{
	switch (method)
	{
		case FH_METHOD_GET:
			return "GET";
		case FH_METHOD_POST:
			return "POST";
		case FH_METHOD_PUT:
			return "PUT";
		case FH_METHOD_DELETE:
			return "DELETE";
		case FH_METHOD_HEAD:
			return "HEAD";
		case FH_METHOD_OPTIONS:
			return "OPTIONS";
		case FH_METHOD_PATCH:
			return "PATCH";
		case FH_METHOD_CONNECT:
			return "CONNECT";
		case FH_METHOD_TRACE:
			return "TRACE";
		default:
			return "Unknown Method";
	}
}

bool
fh_validate_header_name (const char *name, size_t len)
{
	for (size_t i = 0; i < len; i++)
	{
		if (!isalnum (name[i]) && name[i] != '!' && name[i] != '#' && name[i] != '$' && name[i] != '%' && name[i] != '&'
			&& name[i] != '\'' && name[i] != '*' && name[i] != '+' && name[i] != '-' && name[i] != '.' && name[i] != '^'
			&& name[i] != '_' && name[i] != '`' && name[i] != '|' && name[i] != '~')
		{
			return false;
		}
	}

	return true;
}

const char *
fh_get_status_text (enum fh_status code, size_t *len_ptr)
{
	const char *text;
	size_t len;

	switch (code)
	{
		case FH_STATUS_OK:
			text = "OK";
			len = 2;
			break;

		case FH_STATUS_CREATED:
			text = "Created";
			len = 7;
			break;

		case FH_STATUS_ACCEPTED:
			text = "Accepted";
			len = 8;
			break;

		case FH_STATUS_NO_CONTENT:
			text = "No Content";
			len = 10;
			break;

		case FH_STATUS_BAD_REQUEST:
			text = "Bad Request";
			len = 11;
			break;

		case FH_STATUS_UNAUTHORIZED:
			text = "Unauthorized";
			len = 12;
			break;

		case FH_STATUS_FORBIDDEN:
			text = "Forbidden";
			len = 9;
			break;

		case FH_STATUS_NOT_FOUND:
			text = "Not Found";
			len = 9;
			break;

		case FH_STATUS_REQUEST_URI_TOO_LONG:
			text = "Request URI Too Long";
			len = 20;
			break;

		case FH_STATUS_INTERNAL_SERVER_ERROR:
			text = "Internal Server Error";
			len = 21;
			break;

		case FH_STATUS_NOT_IMPLEMENTED:
			text = "Not Implemented";
			len = 15;
			break;

		case FH_STATUS_SERVICE_UNAVAILABLE:
			text = "Service Unavailable";
			len = 19;
			break;

		case FH_STATUS_METHOD_NOT_ALLOWED:
			text = "Method Not Allowed";
			len = 18;
			break;

		default:
			text = "Unknown Status";
			len = 14;

#ifndef NDEBUG
			/* In debug mode, we want to ensure that all cases are handled, so we don't return a default case. */
			assert (false && "Unhandled fh_status code");
			return "Unknown Status"; /* This line will never be reached, but it satisfies the compiler. */
#endif								 /* NDEBUG */
			break;
	}

	if (len_ptr)
		*len_ptr = len;

	return text;
}

const char *
fh_get_status_description (enum fh_status code, size_t *len_ptr)
{
	const char *text;
	size_t len;

	switch (code)
	{
		case FH_STATUS_OK:
			text = "The request has succeeded.";
			len = 26;
			break;

		case FH_STATUS_CREATED:
			text = "The request has been fulfilled and resulted in a new resource being created.";
			len = 76;
			break;

		case FH_STATUS_ACCEPTED:
			text = "The request has been accepted for processing, but the processing has not been completed.";
			len = 88;
			break;

		case FH_STATUS_NO_CONTENT:
			text = "The server successfully processed the request, but is not returning any content.";
			len = 80;
			break;

		case FH_STATUS_BAD_REQUEST:
			text = "The server cannot or will not process the request due to a client error (e.g., malformed request "
				   "syntax).";
			len = 105;
			break;

		case FH_STATUS_UNAUTHORIZED:
			text = "The server could not verify that the client is authorized to access the requested resource.";
			len = 91;
			break;

		case FH_STATUS_FORBIDDEN:
			text = "You don't have permission to access the requested resource on the server.";
			len = 73;
			break;

		case FH_STATUS_NOT_FOUND:
			text = "The requested URL was not found on the server.";
			len = 46;
			break;

		case FH_STATUS_REQUEST_URI_TOO_LONG:
			text = "The request URI is too long for the server to process.";
			len = 54;
			break;

		case FH_STATUS_INTERNAL_SERVER_ERROR:
			text = "The server encountered an unexpected condition that prevented it from fulfilling the request.";
			len = 93;
			break;

		case FH_STATUS_NOT_IMPLEMENTED:
			text = "The server does not support the functionality required to fulfill the request.";
			len = 78;
			break;

		case FH_STATUS_SERVICE_UNAVAILABLE:
			text = "The server is currently unable to handle the request due to temporary overloading or maintenance.";
			len = 97;
			break;

		case FH_STATUS_METHOD_NOT_ALLOWED:
			text = "The request method is not allowed or supported for the requested resource.";
			len = 74;
			break;

		default:
			text = "Additional information is not available for this request.";
			len = 57;
			break;
	}

	if (len_ptr)
		*len_ptr = len;

	return text;
}

static inline struct fh_header *
fh_headers_new_entry (pool_t *pool, struct fh_headers *headers)
{
	struct fh_header *header = fh_pool_alloc (pool, sizeof (struct fh_header));

	if (!header)
		return NULL;

	header->next = NULL;

	if (!headers->tail)
	{
		headers->head = headers->tail = header;
		headers->count = 1;
	}
	else
	{
		headers->tail->next = header;
		headers->tail = header;
		headers->count++;
	}

	return header;
}

struct fh_header *
fh_header_add (pool_t *pool, struct fh_headers *headers, const char *name, size_t name_len, const char *value,
			   size_t value_len)
{
	struct fh_header *header = fh_headers_new_entry (pool, headers);

	if (!header)
		return NULL;

	header->name = name;
	header->name_len = name_len;
	header->value = value;
	header->value_len = value_len;

	return header;
}

struct fh_header *
fh_header_addf (pool_t *pool, struct fh_headers *headers, const char *name, size_t name_len, const char *value_format,
				...)
{
	assert (false);
	return NULL;
}

void
fh_headers_init (struct fh_headers *headers)
{
	memset (headers, 0, sizeof (*headers));
}
