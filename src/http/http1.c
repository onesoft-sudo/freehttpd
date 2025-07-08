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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FH_LOG_MODULE_NAME "http1"

#include "compat.h"
#include "core/stream.h"
#include "http1.h"
#include "log/log.h"
#include "macros.h"
#include "mm/pool.h"
#include "utils/strutils.h"
#include "utils/utils.h"

#define H1_RET(ret) ((1 << 8) | (ret))
#define H1_NEXT 0x0
#define H1_RECV 0x1

static const char *HTTP1_METHOD_LIST[] = {
	[FH_METHOD_GET] = "GET",
	[FH_METHOD_POST] = "POST",
	[FH_METHOD_PUT] = "PUT",
	[FH_METHOD_PATCH] = "PATCH",
	[FH_METHOD_DELETE] = "DELETE",
	[FH_METHOD_HEAD] = "HEAD",
	[FH_METHOD_OPTIONS] = "OPTIONS",
	[FH_METHOD_CONNECT] = "CONNECT",
	[FH_METHOD_TRACE] = "TRACE",
};

static const size_t HTTP1_METHOD_LIST_SIZE = sizeof (HTTP1_METHOD_LIST) / sizeof HTTP1_METHOD_LIST[0];


struct fh_http1_ctx *
fh_http1_ctx_create (struct fh_stream *stream)
{
	struct fh_http1_ctx *ctx = fh_pool_zalloc (stream->pool, sizeof (*ctx));

	if (!ctx)
		return NULL;

	ctx->state = H1_STATE_METHOD;
	ctx->stream = stream;
	ctx->start = ctx->end = stream->head;

	return ctx;
}

static bool
stream_memcpy (void *dest, const struct fh_link *start, size_t start_off, const struct fh_link *end, size_t end_off,
			   size_t total_size)
{
	size_t copied = 0;

	fh_pr_debug ("%s: dest: %p", __func__, dest);
	fh_pr_debug ("%s: start: %p", __func__, (void *) start);
	fh_pr_debug ("%s: start_off: %zu", __func__, start_off);
	fh_pr_debug ("%s: end: %p", __func__, (void *) end);
	fh_pr_debug ("%s: end_off: %zu", __func__, end_off);
	fh_pr_debug ("%s: total_size: %zu", __func__, total_size);

	while (start)
	{
		size_t start_index = start_off;
		size_t end_index = start == end ? end_off : (start->buf->len - 1);
		size_t size = end_index - start_index + 1;

		size = copied + size > total_size ? total_size - copied : size;

		if (size > 0)
		{
			fh_pr_debug ("Copying %zu bytes from a buffer with length: %zu%s", size, start->buf->len,
						 start == end ? " [END]" : "");

			memmove (((uint8_t *) dest) + copied, start->buf->data + start_index, size);
			copied += size;
		}

		start_off = 0;
		start = start->next;
	}

	fh_pr_debug ("Copied: %zu/%zu bytes", copied, total_size);
	return true;
}

static unsigned int
h1_parse_method (struct fh_http1_ctx *ctx, struct fh_conn *conn __attribute_maybe_unused__)
{
	const size_t max_size = HTTP1_METHOD_MAX_LEN + 1; /* For the space */
	ctx->recv_limit = max_size;

	while (ctx->end && ctx->current_consumed_size <= max_size)
	{
		struct fh_buf *buf = ctx->end->buf;
		size_t start_index = ctx->end == ctx->start ? ctx->phase_start_pos + ctx->start_pos : 0;
		size_t end_pos = buf->len;

		if (start_index >= end_pos)
		{
			fh_pr_debug ("Skipped");

			if (!ctx->end->next)
				break;

			ctx->end = ctx->end->next;
			ctx->start_pos = 0;
			continue;
		}

		if (start_index > buf->len || end_pos > buf->len)
		{
			fh_pr_debug ("Overflow: %zu, %zu", start_index, end_pos);
			ctx->state = H1_STATE_ERROR;
			return H1_RET (false);
		}

		size_t size = end_pos - start_index;
		uint8_t *start = buf->data + start_index;
		uint8_t *pos = memchr (start, ' ', size);

		size_t consumed = (pos ? (size_t) (pos - start) : size) - ctx->start_pos;
		ctx->current_consumed_size += consumed;

		fh_pr_debug ("Consumed: %zu bytes [%zu size]", consumed, size);

		if (pos)
		{
			bool single_link = ctx->end == ctx->start;
			size_t method_len;
			const char *method;

			if (single_link)
			{
				method = (const char *) (buf->data + ctx->phase_start_pos);
				method_len = (size_t) (pos - (uint8_t *) method);
			}
			else
			{
				method_len = ctx->current_consumed_size;
			}

			if (method_len == 0)
			{
				fh_pr_debug ("Request method is empty");
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}

			if (method_len > HTTP1_METHOD_MAX_LEN)
			{
				fh_pr_debug ("Request method is too long: %zu", method_len);
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}

			size_t end_pos = start_index + (size_t) (pos - start);

			if (!single_link)
			{
				char *alloced_method = fh_pool_alloc (ctx->stream->pool, method_len);

				if (!alloced_method)
				{
					fh_pr_debug ("Memory allocation failed");
					ctx->state = H1_STATE_ERROR;
					return H1_RET (false);
				}

				stream_memcpy (alloced_method, ctx->start, ctx->phase_start_pos, ctx->end, end_pos > 0 ? end_pos : end_pos,
							   method_len);
				method = alloced_method;
			}

			fh_pr_debug ("Method: (%zu)|%.*s|", method_len, (int) method_len, method);

			bool method_found = false;

			for (size_t i = 0; i < HTTP1_METHOD_LIST_SIZE; i++)
			{
				if (strncmp (method, HTTP1_METHOD_LIST[i], method_len) == 0)
				{
					ctx->result.method = i;
					method_found = true;
					break;
				}
			}

			if (!method_found)
			{
				fh_pr_debug ("Invalid HTTP method");
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}

			ctx->start = ctx->end;
			ctx->phase_start_pos = end_pos + 1;
			ctx->start_pos = 0;
			ctx->state = H1_STATE_URI;

			ctx->total_consumed_size += ctx->current_consumed_size + 1; /* +1 For the space */
			ctx->current_consumed_size = 0;

			return H1_NEXT;
		}

		if (!ctx->end->next)
		{
			ctx->start_pos = size;
			break;
		}

		ctx->start_pos = 0;
		ctx->end = ctx->end->next;
	}

	if (ctx->current_consumed_size > max_size)
	{
		fh_pr_debug ("Request method too long");
		ctx->state = H1_STATE_ERROR;
		return H1_RET (false);
	}

	return H1_RECV;
}

static unsigned int
h1_parse_uri (struct fh_http1_ctx *ctx, struct fh_conn *conn __attribute_maybe_unused__)
{
	const size_t max_size = HTTP1_URI_MAX_LEN + 1; /* For the space */
	ctx->recv_limit = max_size;

	while (ctx->end && ctx->current_consumed_size <= max_size)
	{
		struct fh_buf *buf = ctx->end->buf;
		size_t start_index = ctx->end == ctx->start ? ctx->phase_start_pos + ctx->start_pos : 0;
		size_t end_pos = buf->len;

		if (start_index >= end_pos)
		{
			fh_pr_debug ("Skipped");

			if (!ctx->end->next)
				break;

			ctx->end = ctx->end->next;
			ctx->start_pos = 0;
			continue;
		}

		if (start_index > buf->len || end_pos > buf->len)
		{
			fh_pr_debug ("Overflow: %zu, %zu", start_index, end_pos);
			ctx->state = H1_STATE_ERROR;
			return H1_RET (false);
		}

		size_t size = end_pos - start_index;

		uint8_t *start = buf->data + start_index;
		uint8_t *pos = memchr (start, ' ', size);

		size_t consumed = (pos ? (size_t) (pos - start) : size) - ctx->start_pos;
		ctx->current_consumed_size += consumed;

		fh_pr_debug ("Consumed: %zu bytes [%zu size]", consumed, size);

		if (pos)
		{
			bool single_link = ctx->end == ctx->start;

			if (single_link)
			{
				ctx->result.uri = (const char *) (buf->data + ctx->phase_start_pos);
				ctx->result.uri_len = (size_t) (pos - (uint8_t *) ctx->result.uri);
			}
			else
			{
				ctx->result.uri_len = ctx->current_consumed_size;
			}

			if (ctx->result.uri_len == 0)
			{
				fh_pr_debug ("Request URI is empty");
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}

			if (ctx->result.uri_len > HTTP1_URI_MAX_LEN)
			{
				fh_pr_debug ("Request URI is too long");
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}

			size_t end_pos = start_index + (size_t) (pos - start);

			if (!single_link)
			{
				char *uri = fh_pool_alloc (ctx->stream->pool, ctx->result.uri_len);

				if (!uri)
				{
					fh_pr_debug ("Memory allocation failed");
					ctx->state = H1_STATE_ERROR;
					return H1_RET (false);
				}

				stream_memcpy (uri, ctx->start, ctx->phase_start_pos, ctx->end, end_pos > 0 ? end_pos - 1 : end_pos,
							   ctx->result.uri_len);
				ctx->result.uri = uri;
			}

			fh_pr_debug ("URI: (%zu)|%.*s|", ctx->result.uri_len, (int) ctx->result.uri_len, ctx->result.uri);

			ctx->start = ctx->end;
			ctx->phase_start_pos = end_pos + 1;
			ctx->start_pos = 0;
			ctx->state = H1_STATE_VERSION;

			ctx->total_consumed_size += ctx->current_consumed_size + 1; /* +1 For the space */
			ctx->current_consumed_size = 0;

			return H1_NEXT;
		}

		if (!ctx->end->next)
		{
			ctx->start_pos = size;
			break;
		}

		ctx->start_pos = 0;
		ctx->end = ctx->end->next;
	}

	if (ctx->current_consumed_size > max_size)
	{
		fh_pr_debug ("Request URI too long: %zu", ctx->current_consumed_size);
		ctx->state = H1_STATE_ERROR;
		return H1_RET (false);
	}

	return H1_RECV;
}

static unsigned int
h1_parse_version (struct fh_http1_ctx *ctx, struct fh_conn *conn __attribute_maybe_unused__)
{
	const size_t max_size = HTTP1_VERSION_MAX_LEN + 2; /* For the space */
	ctx->recv_limit = max_size;

	while (ctx->end && ctx->current_consumed_size <= max_size)
	{
		struct fh_buf *buf = ctx->end->buf;
		size_t start_index = ctx->end == ctx->start ? ctx->phase_start_pos + ctx->start_pos : 0;
		size_t end_pos = buf->len;

		if (start_index >= end_pos)
		{
			fh_pr_debug ("Skipped");

			if (!ctx->end->next)
				break;

			ctx->end = ctx->end->next;
			ctx->start_pos = 0;
			continue;
		}

		if (start_index > buf->len || end_pos > buf->len)
		{
			fh_pr_debug ("Overflow: %zu, %zu", start_index, end_pos);
			ctx->state = H1_STATE_ERROR;
			return H1_RET (false);
		}

		size_t size = end_pos - start_index;

		uint8_t *start = buf->data + start_index;
		uint8_t *pos = memchr (start, '\r', size);

		size_t consumed = (pos ? (size_t) (pos - start) : size) - ctx->start_pos;
		ctx->current_consumed_size += consumed;

		fh_pr_debug ("Consumed: %zu bytes [%zu size]", consumed, size);

		if (pos)
		{
			bool single_link = ctx->end == ctx->start;

			if (single_link)
			{
				ctx->result.version = (const char *) (buf->data + ctx->phase_start_pos);
				ctx->result.version_len = (size_t) (pos - (uint8_t *) ctx->result.version);
			}
			else
			{
				ctx->result.version_len = ctx->current_consumed_size;
			}

			if (ctx->result.version_len + 2 > size || *pos != '\r' || pos[1] != '\n')
			{
				fh_pr_debug ("Expected newline after version");
				goto h1_version_skip_iter;
			}

			if (ctx->result.version_len == 0 || ctx->result.version_len > HTTP1_VERSION_MAX_LEN)
			{
				fh_pr_debug ("Version is empty or invalid");
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}

			size_t end_pos = start_index + (size_t) (pos - start);

			if (!single_link)
			{
				char *version = fh_pool_alloc (ctx->stream->pool, ctx->result.version_len);

				if (!version)
				{
					fh_pr_debug ("Memory allocation failed");
					ctx->state = H1_STATE_ERROR;
					return H1_RET (false);
				}

				stream_memcpy (version, ctx->start, ctx->phase_start_pos, ctx->end, end_pos > 0 ? end_pos - 1 : end_pos,
							   ctx->result.version_len);
				ctx->result.version = version;
			}

			fh_pr_debug ("Version: (%zu)|%.*s|", ctx->result.version_len, (int) ctx->result.version_len, ctx->result.version);

			ctx->start = ctx->end;
			ctx->phase_start_pos = end_pos + 2;
			ctx->start_pos = 0;
			ctx->state = H1_STATE_HEADER_NAME;

			ctx->total_consumed_size += ctx->current_consumed_size + 2; /* +2 For the "\r\n" */
			ctx->current_consumed_size = 0;

			return H1_NEXT;
		}

	h1_version_skip_iter:
		if (!ctx->end->next)
		{
			ctx->start_pos = size;
			break;
		}

		ctx->start_pos = 0;
		ctx->end = ctx->end->next;
	}

	if (ctx->current_consumed_size > max_size)
	{
		fh_pr_debug ("Version too long: %zu", ctx->current_consumed_size);
		ctx->state = H1_STATE_ERROR;
		return H1_RET (false);
	}

	return H1_RECV;
}

static unsigned int
h1_parse_header_name (struct fh_http1_ctx *ctx, struct fh_conn *conn __attribute_maybe_unused__)
{
	const size_t max_size = HTTP1_HEADER_NAME_MAX_LEN + 1; /* For the ':' */
	ctx->recv_limit = max_size;

	while (ctx->end && ctx->current_consumed_size <= max_size)
	{
		struct fh_buf *buf = ctx->end->buf;
		size_t start_index = ctx->end == ctx->start ? ctx->phase_start_pos + ctx->start_pos : 0;
		size_t end_pos = buf->len;

		if (start_index >= end_pos)
		{
			fh_pr_debug ("Skipped");

			if (!ctx->end->next)
				break;

			ctx->end = ctx->end->next;
			ctx->start_pos = 0;
			continue;
		}

		if (start_index > buf->len || end_pos > buf->len)
		{
			fh_pr_debug ("Overflow: %zu, %zu", start_index, end_pos);
			ctx->state = H1_STATE_ERROR;
			return H1_RET (false);
		}

		size_t size = end_pos - start_index;

		uint8_t *start = buf->data + start_index;
		uint8_t *pos = memchr (start, ':', size);

		if (size >= 2 && start[0] == '\r' && start[1] == '\n')
		{
			fh_pr_debug ("Received \\r\\n twice, marking end of headers (EOH)");
			ctx->state = H1_STATE_DONE;

			ctx->start = ctx->end;
			ctx->phase_start_pos = end_pos + 2;
			ctx->start_pos = 0;

			ctx->total_consumed_size += 2;
			ctx->current_consumed_size = 0;

			return H1_NEXT;
		}

		size_t consumed = (pos ? (size_t) (pos - start) : size) - ctx->start_pos;
		ctx->current_consumed_size += consumed;

		fh_pr_debug ("Consumed: %zu bytes [%zu size]", consumed, size);

		if (pos)
		{
			bool single_link = ctx->end == ctx->start;

			if (single_link)
			{
				ctx->result.current_header_name = (const char *) (buf->data + ctx->phase_start_pos);
				ctx->result.current_header_name_len = (size_t) (pos - (uint8_t *) ctx->result.current_header_name);
			}
			else
			{
				ctx->result.current_header_name_len = ctx->current_consumed_size;
			}

			if (ctx->result.current_header_name_len == 0)
			{
				fh_pr_debug ("Header name is empty");
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}

			if (ctx->result.current_header_name_len > HTTP1_HEADER_NAME_MAX_LEN)
			{
				fh_pr_debug ("Header name is too long");
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}

			size_t end_pos = start_index + (size_t) (pos - start);

			if (!single_link)
			{
				char *current_header_name = fh_pool_alloc (ctx->stream->pool, ctx->result.current_header_name_len);

				if (!current_header_name)
				{
					fh_pr_debug ("Memory allocation failed");
					ctx->state = H1_STATE_ERROR;
					return H1_RET (false);
				}

				stream_memcpy (current_header_name, ctx->start, ctx->phase_start_pos, ctx->end,
							   end_pos > 0 ? end_pos - 1 : end_pos, ctx->result.current_header_name_len);
				ctx->result.current_header_name = current_header_name;
			}

			fh_pr_debug ("[-] Current Header name: (%zu)|%.*s|", ctx->result.current_header_name_len,
						 (int) ctx->result.current_header_name_len, ctx->result.current_header_name);

			ctx->start = ctx->end;
			ctx->phase_start_pos = end_pos + 1;
			ctx->start_pos = 0;
			ctx->state = H1_STATE_HEADER_VALUE;

			ctx->total_consumed_size += ctx->current_consumed_size + 1; /* +1 For the ":" */
			ctx->current_consumed_size = 0;

			return H1_NEXT;
		}

		if (!ctx->end->next)
		{
			ctx->start_pos = size;
			break;
		}

		ctx->start_pos = 0;
		ctx->end = ctx->end->next;
	}

	if (ctx->current_consumed_size > max_size)
	{
		fh_pr_debug ("Version too long: %zu", ctx->current_consumed_size);
		ctx->state = H1_STATE_ERROR;
		return H1_RET (false);
	}

	return H1_RECV;
}

static unsigned int
h1_parse_header_value (struct fh_http1_ctx *ctx, struct fh_conn *conn __attribute_maybe_unused__)
{
	const size_t max_size = HTTP1_HEADER_VALUE_MAX_LEN + 2; /* For the '\r\n' */
	ctx->recv_limit = max_size;

	while (ctx->end && ctx->current_consumed_size <= max_size)
	{
		struct fh_buf *buf = ctx->end->buf;
		size_t start_index = ctx->end == ctx->start ? ctx->phase_start_pos + ctx->start_pos : 0;
		size_t end_pos = buf->len;

		if (start_index >= end_pos)
		{
			fh_pr_debug ("Skipped");

			if (!ctx->end->next)
				break;

			ctx->end = ctx->end->next;
			ctx->start_pos = 0;
			continue;
		}

		if (start_index > buf->len || end_pos > buf->len)
		{
			fh_pr_debug ("Overflow: %zu, %zu", start_index, end_pos);
			ctx->state = H1_STATE_ERROR;
			return H1_RET (false);
		}

		size_t size = end_pos - start_index;

		uint8_t *start = buf->data + start_index;
		uint8_t *pos = memchr (start, '\r', size);

		size_t consumed = (pos ? (size_t) (pos - start) : size) - ctx->start_pos;
		ctx->current_consumed_size += consumed;

		fh_pr_debug ("Consumed: %zu bytes [%zu size]", consumed, size);

		if (pos)
		{
			bool single_link = ctx->end == ctx->start;
			size_t value_len;
			const char *value;

			if (single_link)
			{
				value = (const char *) (buf->data + ctx->phase_start_pos);
				value_len = (size_t) (pos - (uint8_t *) value);
			}
			else
			{
				value_len = ctx->current_consumed_size;
			}

			if (value_len + 2 > size || *pos != '\r' || pos[1] != '\n')
			{
				fh_pr_debug ("Expected newline after header value");
				goto h1_header_value_skip_iter;
			}

			if (value_len == 0)
			{
				fh_pr_debug ("Header value is empty");
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}

			if (value_len > HTTP1_HEADER_VALUE_MAX_LEN)
			{
				fh_pr_debug ("Header value is too long");
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}

			size_t end_pos = start_index + (size_t) (pos - start);

			if (!single_link)
			{
				char *alloc_value = fh_pool_alloc (ctx->stream->pool, value_len);

				if (!alloc_value)
				{
					fh_pr_debug ("Memory allocation failed");
					ctx->state = H1_STATE_ERROR;
					return H1_RET (false);
				}

				stream_memcpy (alloc_value, ctx->start, ctx->phase_start_pos, ctx->end,
							   end_pos > 0 ? end_pos - 1 : end_pos, value_len);
				value = alloc_value;
			}

			size_t trimmed_value_len = 0;
			value = str_trim_whitespace (value, value_len, &trimmed_value_len);
			value_len = trimmed_value_len;

			fh_pr_debug ("[-] Current Header value: (%zu)|%.*s|", value_len, (int) value_len, value);

			if (!fh_header_add (ctx->stream->pool, &ctx->result.headers, ctx->result.current_header_name, ctx->result.current_header_name_len, value,
								value_len))
			{
				fh_pr_debug ("Memory allocation failed");
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}

			ctx->start = ctx->end;
			ctx->phase_start_pos = end_pos + 2;
			ctx->start_pos = 0;
			ctx->state = H1_STATE_HEADER_NAME;

			ctx->total_consumed_size += ctx->current_consumed_size + 2; /* +2 For the "\r\n" */
			ctx->current_consumed_size = 0;

			return H1_NEXT;
		}

	h1_header_value_skip_iter:
		if (!ctx->end->next)
		{
			ctx->start_pos = size;
			break;
		}

		ctx->start_pos = 0;
		ctx->end = ctx->end->next;
	}

	if (ctx->current_consumed_size > max_size)
	{
		fh_pr_debug ("Version too long: %zu", ctx->current_consumed_size);
		ctx->state = H1_STATE_ERROR;
		return H1_RET (false);
	}

	return H1_RECV;
}

static unsigned int
h1_recv (struct fh_http1_ctx *ctx, struct fh_conn *conn)
{
	const size_t MAX_RECV_AT_ONCE = ctx->recv_limit;
	const size_t CAPACITY = MAX_RECV_AT_ONCE > 4096 ? MAX_RECV_AT_ONCE : 4096;
	// const size_t CAPACITY = 15;
	size_t size = 0;
	bool read = false;

	while (size < MAX_RECV_AT_ONCE)
	{
		// size_t readable_size = size + CAPACITY > MAX_RECV_AT_ONCE ? MAX_RECV_AT_ONCE - size : CAPACITY;
		size_t readable_size = CAPACITY;
		uint8_t *buf;
		bool allocated = false;

		if (conn->stream->tail && conn->stream->tail->buf->len < conn->stream->tail->buf->cap)
		{
			buf = conn->stream->tail->buf->data + conn->stream->tail->buf->len;
			readable_size = conn->stream->tail->buf->cap - conn->stream->tail->buf->len;
			// readable_size = size + readable_size > ctx->recv_limit ? MAX_RECV_AT_ONCE - readable_size :
			// readable_size;
		}
		else
		{
			buf = fh_pool_alloc (conn->pool, CAPACITY);
			allocated = true;

			if (!buf)
			{
				fh_pr_debug ("connection %lu: buffer allocation failed: %s", conn->id, strerror (errno));
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}
		}

		ssize_t bytes_received = recv (conn->client_sockfd, buf, readable_size, 0);

		if (bytes_received < 0)
		{
			if (allocated)
				fh_pool_undo_last_alloc (conn->pool, CAPACITY);

			if (would_block ())
			{
				if (!read)
					return H1_RET (true);

				break;
			}

			fh_pr_debug ("connection %lu: I/O error: %s", conn->id, strerror (errno));
			ctx->state = H1_STATE_ERROR;
			return H1_RET (false);
		}

		if (bytes_received == 0)
		{
			if (allocated)
				fh_pool_undo_last_alloc (conn->pool, CAPACITY);

			if (!read)
				return H1_RET (true);

			break;
		}

		read = true;
		size += (size_t) bytes_received;

		if (allocated)
		{
			if (!fh_stream_add_buf_data (ctx->stream, buf, (size_t) bytes_received, CAPACITY))
			{
				fh_pr_debug ("connection %lu: allocation error: %s", conn->id, strerror (errno));
				ctx->state = H1_STATE_ERROR;
				return H1_RET (false);
			}

			if (!ctx->start && !ctx->end)
			{
				ctx->start = ctx->stream->head;
				ctx->end = ctx->stream->tail;
			}
		}
		else
		{
			conn->stream->tail->buf->len += (size_t) bytes_received;
			fh_pr_debug ("connection %lu: reused buffer: %zu bytes read, %zu capacity", conn->id, readable_size,
						 conn->stream->tail->buf->cap);
		}

		fh_pr_debug ("connection %lu: received %zu bytes", conn->id, (size_t) bytes_received);
	}

	ctx->state = ctx->prev_state;
	return H1_NEXT;
}

bool
fh_http1_parse (struct fh_http1_ctx *ctx, struct fh_conn *conn)
{
	for (;;)
	{
		unsigned int ret;

		switch (ctx->state)
		{
			case H1_STATE_DONE:
				return true;

			case H1_STATE_METHOD:
				ret = h1_parse_method (ctx, conn);
				break;

			case H1_STATE_URI:
				ret = h1_parse_uri (ctx, conn);
				break;

			case H1_STATE_VERSION:
				ret = h1_parse_version (ctx, conn);
				break;

			case H1_STATE_HEADER_NAME:
				ret = h1_parse_header_name (ctx, conn);
				break;

			case H1_STATE_HEADER_VALUE:
				ret = h1_parse_header_value (ctx, conn);
				break;

			case H1_STATE_RECV:
				ret = h1_recv (ctx, conn);
				break;

			default:
				fh_pr_err ("Invalid state: %d", ctx->state);
				return false;
		}

		if (ret >> 8U)
			return (bool) (ret & 0xFF);

		if (ret == H1_RECV)
		{
			ctx->prev_state = ctx->state;
			ctx->state = H1_STATE_RECV;
			continue;
		}
	}

	return true;
}

bool
fh_http1_is_done (const struct fh_http1_ctx *ctx)
{
	return ctx->state == H1_STATE_DONE;
}
