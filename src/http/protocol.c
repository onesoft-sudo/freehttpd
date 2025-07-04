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
#include "protocol.h"

#define STREAM_DETECT_INITIAL_READ_SIZE 64

static_assert (sizeof (H2_PREFACE) - 1 <= STREAM_DETECT_INITIAL_READ_SIZE,
			   "HTTP2_PREFACE size exceeds initial read buffer size");

const char *
fhttpd_protocol_to_string (enum fhttpd_protocol protocol)
{
	switch (protocol)
	{
		case FHTTPD_PROTOCOL_HTTP_1X:
			return "HTTP/1.x";
		case FHTTPD_PROTOCOL_H2:
			return "h2";
		default:
			return "Unknown Protocol";
	}
}

enum fhttpd_protocol
fhttpd_string_to_protocol (const char *protocol_str)
{
	if (strcmp (protocol_str, "HTTP/1.0") == 0 || strcmp (protocol_str, "HTTP/1.1") == 0)
		return FHTTPD_PROTOCOL_HTTP_1X;
	else if (strcmp (protocol_str, "HTTP/2.0") == 0 || strcmp (protocol_str, "h2") == 0
			 || strcmp (protocol_str, "h2c") == 0)
		return FHTTPD_PROTOCOL_H2;
	else
		return -1;
}

const char *
fhttpd_method_to_string (enum fhttpd_method method)
{
	switch (method)
	{
		case FHTTPD_METHOD_GET:
			return "GET";
		case FHTTPD_METHOD_POST:
			return "POST";
		case FHTTPD_METHOD_PUT:
			return "PUT";
		case FHTTPD_METHOD_DELETE:
			return "DELETE";
		case FHTTPD_METHOD_HEAD:
			return "HEAD";
		case FHTTPD_METHOD_OPTIONS:
			return "OPTIONS";
		case FHTTPD_METHOD_PATCH:
			return "PATCH";
		case FHTTPD_METHOD_CONNECT:
			return "CONNECT";
		case FHTTPD_METHOD_TRACE:
			return "TRACE";
		default:
			return "Unknown Method";
	}
}

bool
fhttpd_validate_header_name (const char *name, size_t len)
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
fhttpd_get_status_text (enum fhttpd_status code)
{
	switch (code)
	{
		case FHTTPD_STATUS_OK:
			return "OK";

		case FHTTPD_STATUS_CREATED:
			return "Created";

		case FHTTPD_STATUS_ACCEPTED:
			return "Accepted";

		case FHTTPD_STATUS_NO_CONTENT:
			return "No Content";

		case FHTTPD_STATUS_BAD_REQUEST:
			return "Bad Request";

		case FHTTPD_STATUS_UNAUTHORIZED:
			return "Unauthorized";

		case FHTTPD_STATUS_FORBIDDEN:
			return "Forbidden";

		case FHTTPD_STATUS_NOT_FOUND:
			return "Not Found";

		case FHTTPD_STATUS_REQUEST_URI_TOO_LONG:
			return "Request URI Too Long";

		case FHTTPD_STATUS_INTERNAL_SERVER_ERROR:
			return "Internal Server Error";

		case FHTTPD_STATUS_NOT_IMPLEMENTED:
			return "Not Implemented";

		case FHTTPD_STATUS_SERVICE_UNAVAILABLE:
			return "Service Unavailable";

		case FHTTPD_STATUS_METHOD_NOT_ALLOWED:
			return "Method Not Allowed";

			/* Only put a default case if building in non-debug mode, so that the compiler can warn about missing
			   cases in debug mode. */
#ifdef NDEBUG
		default:
			return "Unknown Status";
#endif /* NDEBUG */
	}

#ifndef NDEBUG
	/* In debug mode, we want to ensure that all cases are handled, so we don't return a default case. */
	assert (false && "Unhandled fhttpd_status code");
	return "Unknown Status"; /* This line will never be reached, but it satisfies the compiler. */
#endif						 /* NDEBUG */
}

const char *
fhttpd_get_status_description (enum fhttpd_status code)
{
	switch (code)
	{
		case FHTTPD_STATUS_OK:
			return "The request has succeeded.";

		case FHTTPD_STATUS_CREATED:
			return "The request has been fulfilled and resulted in a new resource being created.";

		case FHTTPD_STATUS_ACCEPTED:
			return "The request has been accepted for processing, but the processing has not been completed.";

		case FHTTPD_STATUS_NO_CONTENT:
			return "The server successfully processed the request, but is not returning any content.";

		case FHTTPD_STATUS_BAD_REQUEST:
			return "The server cannot or will not process the request due to a client error (e.g., malformed request "
				   "syntax).";

		case FHTTPD_STATUS_UNAUTHORIZED:
			return "The server could not verify that the client is authorized to access the requested resource.";

		case FHTTPD_STATUS_FORBIDDEN:
			return "You don't have permission to access the requested resource on the server.";

		case FHTTPD_STATUS_NOT_FOUND:
			return "The requested URL was not found on the server.";

		case FHTTPD_STATUS_REQUEST_URI_TOO_LONG:
			return "The request URI is too long for the server to process.";

		case FHTTPD_STATUS_INTERNAL_SERVER_ERROR:
			return "The server encountered an unexpected condition that prevented it from fulfilling the request.";

		case FHTTPD_STATUS_NOT_IMPLEMENTED:
			return "The server does not support the functionality required to fulfill the request.";

		case FHTTPD_STATUS_SERVICE_UNAVAILABLE:
			return "The server is currently unable to handle the request due to temporary overloading or maintenance.";

		case FHTTPD_STATUS_METHOD_NOT_ALLOWED:
			return "The request method is not allowed or supported for the requested resource.";

			/* Only put a default case if building in non-debug mode, so that the compiler can warn about missing
			   cases in debug mode. */
#ifdef NDEBUG
		default:
			return "Additional information is not available for this request.";
#endif /* NDEBUG */
	}

#ifndef NDEBUG
	/* In debug mode, we want to ensure that all cases are handled, so we don't return a default case. */
	assert (false && "Unhandled fhttpd_status code");
	return "Unknown Status"; /* This line will never be reached, but it satisfies the compiler. */
#endif						 /* NDEBUG */
}

bool
fhttpd_header_add (struct fhttpd_headers *headers, const char *name, const char *value, size_t name_length,
				   size_t value_length)
{
	struct fhttpd_header *list = realloc (headers->list, sizeof (struct fhttpd_header) * (headers->count + 1));

	if (!list)
		return false;

	headers->list = list;
	return fhttpd_header_add_noalloc (headers, headers->count, name, value, name_length, value_length);
}

bool
fhttpd_header_add_noalloc (struct fhttpd_headers *headers, size_t index, const char *name, const char *value,
						   size_t name_length, size_t value_length)
{
	headers->list[index].name_length = name_length == 0 ? strlen (name) : name_length;
	headers->list[index].value_length = value_length == 0 ? strlen (value) : value_length;
	headers->list[index].name = strndup (name, headers->list[index].name_length);
	headers->list[index].value = strndup (value, headers->list[index].value_length);
	headers->count++;
	return headers->list[index].name && headers->list[index].value;
}

void
fhttpd_request_free (struct fhttpd_request *request, bool inner_only)
{
	free (request->full_host);
	free (request->path);
	free (request->qs);
	free (request->uri);
	free (request->host);

	if (request->headers.list)
	{
		for (size_t j = 0; j < request->headers.count; j++)
		{
			free (request->headers.list[j].name);
			free (request->headers.list[j].value);
		}

		free (request->headers.list);
	}

	free (request->body);

	if (!inner_only)
		free (request);
}

bool
fhttpd_header_add_noalloc_printf (struct fhttpd_headers *headers, size_t index, const char *name, size_t name_length,
								  const char *value_format, ...)
{
	char *value = NULL;

	va_list args;
	va_start (args, value_format);
	int value_len = vasprintf (&value, value_format, args);
	va_end (args);

	if (value_len < 0 || !value)
		return false;

	headers->list[index].name_length = name_length == 0 ? strlen (name) : name_length;
	headers->list[index].name = strndup (name, headers->list[index].name_length);
	headers->list[index].value = value;
	headers->list[index].value_length = (size_t) value_len;
	headers->count++;

	return headers->list[index].name && headers->list[index].value;
}

bool
fhttpd_header_add_printf (struct fhttpd_headers *headers, const char *name, size_t name_length,
						  const char *value_format, ...)
{
	char *value = NULL;

	va_list args;
	va_start (args, value_format);
	int value_len = vasprintf (&value, value_format, args);
	va_end (args);

	if (value_len < 0 || !value)
		return false;

	struct fhttpd_header *list = realloc (headers->list, sizeof (struct fhttpd_header) * (headers->count + 1));

	if (!list)
		return false;

	headers->list = list;
	headers->list[headers->count].name_length = name_length == 0 ? strlen (name) : name_length;
	headers->list[headers->count].name = strndup (name, headers->list[headers->count].name_length);
	headers->list[headers->count].value = value;
	headers->list[headers->count].value_length = (size_t) value_len;
	headers->count++;

	return headers->list[headers->count - 1].name && headers->list[headers->count - 1].value;
}
