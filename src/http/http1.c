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
#include <strings.h>
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

#define H1_NEXT 0x0
#define H1_RECV 0x1
#define H1_DONE 0x2
#define H1_RET(ret) ((1U << 8U) | (ret))
#define H1_ERR(code) ((1U << 31U) | (code))

static const char *HTTP1_METHOD_LIST[] = {
	[FH_METHOD_GET] = "GET",		 [FH_METHOD_POST] = "POST",		  [FH_METHOD_PUT] = "PUT",
	[FH_METHOD_PATCH] = "PATCH",	 [FH_METHOD_DELETE] = "DELETE",	  [FH_METHOD_HEAD] = "HEAD",
	[FH_METHOD_OPTIONS] = "OPTIONS", [FH_METHOD_CONNECT] = "CONNECT", [FH_METHOD_TRACE] = "TRACE",
};

static const size_t HTTP1_METHOD_LEN_LIST[] = {
	[FH_METHOD_GET] = 3,	 [FH_METHOD_POST] = 4,	  [FH_METHOD_PUT] = 4,
	[FH_METHOD_PATCH] = 5,	 [FH_METHOD_DELETE] = 6,  [FH_METHOD_HEAD] = 4,
	[FH_METHOD_OPTIONS] = 7, [FH_METHOD_CONNECT] = 7, [FH_METHOD_TRACE] = 5,
};

static const size_t HTTP1_METHOD_LIST_SIZE = sizeof (HTTP1_METHOD_LIST) / sizeof HTTP1_METHOD_LIST[0];

const size_t DEFAULT_BUF_SIZE = 4096;

struct fh_http1_ctx *
fh_http1_ctx_create (struct fh_stream *stream)
{
	struct fh_http1_ctx *ctx = fh_pool_zalloc (stream->pool, sizeof (*ctx));

	if (!ctx)
		return NULL;

	ctx->state = H1_STATE_METHOD;
	ctx->stream = stream;
	ctx->cur.link = stream->head;

	return ctx;
}

static unsigned int
fh_http1_parse_method (struct fh_http1_ctx *ctx)
{
	struct fh_http1_cursor *cur = &ctx->arg_cur;

	while (cur->link && ctx->current_consumed <= HTTP1_METHOD_MAX_LEN)
	{
		struct fh_buf *buf = cur->link->buf;

		if (cur->off < buf->len)
		{
			size_t len = buf->len - cur->off;
			uint8_t *start_ptr = buf->data + cur->off;
			uint8_t *pos_ptr = memchr (start_ptr, ' ', len);

			if (pos_ptr)
			{
				size_t memchr_consumed = (size_t) (pos_ptr - start_ptr);

				cur->off += memchr_consumed;
				ctx->current_consumed += memchr_consumed;

				const char *method;
				bool is_fragmented = ctx->cur.link != cur->link;
				size_t eff_len = is_fragmented ? memchr_consumed : ctx->current_consumed;

				if (eff_len == 0)
				{
					fh_pr_debug ("Method is empty");
					return H1_ERR (400);
				}

				if (eff_len > HTTP1_METHOD_MAX_LEN)
				{
					fh_pr_debug ("Method is too long");
					return H1_ERR (400);
				}

				if (is_fragmented)
				{
					char *method_buf = fh_pool_alloc (ctx->stream->pool, eff_len);

					if (!method_buf)
					{
						fh_pr_debug ("Failed to allocate memory");
						return H1_ERR (500);
					}

					size_t copied
						= fh_stream_copy (method_buf, ctx->cur.link, ctx->cur.off, cur->link, cur->off, eff_len);

					if (copied != eff_len)
					{
						fh_pr_debug ("fh_stream_copy() copied %zu/%zu bytes", copied, eff_len);
						return H1_ERR (500);
					}

					fh_pr_debug ("fh_stream_copy() successfully copied %zu bytes", copied);
					method = method_buf;
				}
				else
				{
					method = (const char *) (start_ptr);
				}

				bool method_found = false;

				for (size_t i = 0; i < HTTP1_METHOD_LIST_SIZE; i++)
				{
					if (memcmp (method, HTTP1_METHOD_LIST[i],
								HTTP1_METHOD_LEN_LIST[i] < eff_len ? HTTP1_METHOD_LEN_LIST[i] : eff_len)
						== 0)
					{
						ctx->request.method = i;
						method_found = true;
						break;
					}
				}

				if (!method_found)
				{
					fh_pr_debug ("Invalid request method");
					return H1_ERR (400);
				}

				ctx->arg_cur.link = ctx->cur.link = cur->link;
				ctx->arg_cur.off = ctx->cur.off = cur->off + 1; /* For the space */
				ctx->state = H1_STATE_URI;
				ctx->total_consumed += ctx->current_consumed + 1;
				ctx->current_consumed = 0;

				return H1_NEXT;
			}
			else
			{
				cur->off += len;
				ctx->current_consumed += len;
			}
		}

		if (!cur->link->next)
			break;

		cur->link = cur->link->next;
		cur->off = 0;
	}

	if (ctx->current_consumed > HTTP1_METHOD_MAX_LEN)
	{
		fh_pr_debug ("Method is too long");
		return H1_ERR (400);
	}

	return H1_RECV;
}

static unsigned int
fh_http1_parse_uri (struct fh_http1_ctx *ctx)
{
	struct fh_http1_cursor *cur = &ctx->arg_cur;

	while (cur->link && ctx->current_consumed <= HTTP1_URI_MAX_LEN)
	{
		struct fh_buf *buf = cur->link->buf;

		if (cur->off < buf->len)
		{
			size_t len = buf->len - cur->off;
			uint8_t *start_ptr = buf->data + cur->off;
			uint8_t *pos_ptr = memchr (start_ptr, ' ', len);

			if (pos_ptr)
			{
				size_t memchr_consumed = (size_t) (pos_ptr - start_ptr);

				cur->off += memchr_consumed;
				ctx->current_consumed += memchr_consumed;

				const char *uri;
				bool is_fragmented = ctx->cur.link != cur->link;
				size_t eff_len = is_fragmented ? memchr_consumed : ctx->current_consumed;

				if (eff_len == 0)
				{
					fh_pr_debug ("URI is empty");
					return H1_ERR (400);
				}

				if (eff_len > HTTP1_URI_MAX_LEN)
				{
					fh_pr_debug ("URI is too long");
					return H1_ERR (400);
				}

				if (is_fragmented)
				{
					char *uri_buf = fh_pool_alloc (ctx->stream->pool, eff_len);

					if (!uri_buf)
					{
						fh_pr_debug ("Failed to allocate memory");
						return H1_ERR (500);
					}

					size_t copied = fh_stream_copy (uri_buf, ctx->cur.link, ctx->cur.off, cur->link, cur->off, eff_len);

					if (copied != eff_len)
					{
						fh_pr_debug ("fh_stream_copy() copied %zu/%zu bytes", copied, eff_len);
						return H1_ERR (500);
					}

					fh_pr_debug ("fh_stream_copy() successfully copied %zu bytes", copied);
					uri = uri_buf;
				}
				else
				{
					uri = (const char *) (start_ptr);
				}

				ctx->request.uri = uri;
				ctx->request.uri_len = eff_len;

				ctx->arg_cur.link = ctx->cur.link = cur->link;
				ctx->arg_cur.off = ctx->cur.off = cur->off + 1; /* For the space */
				ctx->state = H1_STATE_VERSION;
				ctx->total_consumed += ctx->current_consumed + 1;
				ctx->current_consumed = 0;

				return H1_NEXT;
			}
			else
			{
				cur->off += len;
				ctx->current_consumed += len;
			}
		}

		if (!cur->link->next)
			break;

		cur->link = cur->link->next;
		cur->off = 0;
	}

	if (ctx->current_consumed > HTTP1_URI_MAX_LEN)
	{
		fh_pr_debug ("URI is too long");
		return H1_ERR (400);
	}

	return H1_RECV;
}

static unsigned int
fh_http1_parse_version (struct fh_http1_ctx *ctx)
{
	struct fh_http1_cursor *cur = &ctx->arg_cur;

	while (cur->link && ctx->current_consumed <= HTTP1_VERSION_MAX_LEN + 1)
	{
		struct fh_buf *buf = cur->link->buf;

		if (cur->off < buf->len)
		{
			size_t len = buf->len - cur->off;
			uint8_t *start_ptr = buf->data + cur->off;
			uint8_t *pos_ptr = memchr (start_ptr, '\n', len);

			if (pos_ptr)
			{
				size_t memchr_consumed = (size_t) (pos_ptr - start_ptr);

				cur->off += memchr_consumed;
				ctx->current_consumed += memchr_consumed;

				const char *version;
				bool is_fragmented = ctx->cur.link != cur->link;
				size_t eff_len = is_fragmented ? memchr_consumed : ctx->current_consumed;

				if (eff_len != HTTP1_VERSION_MAX_LEN + 1)
				{
					fh_pr_debug ("Version is invalid");
					return H1_ERR (400);
				}

				if (is_fragmented)
				{
					char *version_buf = fh_pool_alloc (ctx->stream->pool, eff_len);

					if (!version_buf)
					{
						fh_pr_debug ("Failed to allocate memory");
						return H1_ERR (500);
					}

					size_t copied
						= fh_stream_copy (version_buf, ctx->cur.link, ctx->cur.off, cur->link, cur->off, eff_len);

					if (copied != eff_len)
					{
						fh_pr_debug ("fh_stream_copy() copied %zu/%zu bytes", copied, eff_len);
						return H1_ERR (500);
					}

					fh_pr_debug ("fh_stream_copy() successfully copied %zu bytes", copied);
					version = version_buf;
				}
				else
				{
					version = (const char *) (start_ptr);
				}

				if (version[eff_len - 1] != '\r')
				{
					fh_pr_debug ("Version does not end with '\\r\\n'");
					return H1_ERR (400);
				}

				eff_len--;

				if (memcmp (version, "HTTP/", 5) != 0)
				{
					fh_pr_debug ("Version does not start with 'HTTP/'");
					return H1_ERR (400);
				}

				char v1 = version[5];
				char v2 = version[6];
				char v3 = version[7];

				if (v2 != '.' || v1 != '1' || (v3 != '1' && v3 != '0'))
				{
					fh_pr_debug ("Invalid HTTP version: |%.*s|", 8, version);
					return H1_ERR (400);
				}

				ctx->request.protocol = v3 == '0' ? FH_PROTOCOL_HTTP_1_0 : FH_PROTOCOL_HTTP_1_1;

				ctx->arg_cur.link = ctx->cur.link = cur->link;
				ctx->arg_cur.off = ctx->cur.off = cur->off + 1; /* For the '\n' */
				ctx->state = H1_STATE_HEADER_NAME;
				ctx->total_consumed += ctx->current_consumed + 1;
				ctx->current_consumed = 0;

				return H1_NEXT;
			}
			else
			{
				cur->off += len;
				ctx->current_consumed += len;
			}
		}

		if (!cur->link->next)
			break;

		cur->link = cur->link->next;
		cur->off = 0;
	}

	if (ctx->current_consumed > HTTP1_VERSION_MAX_LEN + 1)
	{
		fh_pr_debug ("Version is too long");
		return H1_ERR (400);
	}

	return H1_RECV;
}

static unsigned int
fh_http1_parse_header_name (struct fh_http1_ctx *ctx)
{
	struct fh_http1_cursor *cur = &ctx->arg_cur;
	bool check_end = true;

	while (cur->link && ctx->current_consumed <= HTTP1_HEADER_NAME_MAX_LEN)
	{
		struct fh_buf *buf = cur->link->buf;

		if (cur->off < buf->len)
		{
			size_t len = buf->len - cur->off;
			uint8_t *start_ptr = buf->data + cur->off;

			if (check_end)
			{
				bool is_fragmented = ctx->cur.link != cur->link;
				bool found = false;
				char end[2];

				if (is_fragmented)
				{
					size_t copied = fh_stream_copy (end, ctx->cur.link, ctx->cur.off, cur->link, cur->off + 2, 2);

					if (copied == 2)
					{
						found = true;
					}
				}
				else if (len >= 2)
				{
					end[0] = start_ptr[0];
					end[1] = start_ptr[1];
					found = true;
				}

				if (found && end[0] == '\r' && end[1] == '\n')
				{
					bool no_body = ctx->request.method == FH_METHOD_GET || ctx->request.method == FH_METHOD_HEAD;

					fh_pr_debug ("\\r\\n received, switching to %s state", no_body ? "done" : "body");

					ctx->arg_cur.link = ctx->cur.link = cur->link;
					ctx->arg_cur.off = ctx->cur.off = cur->off + 2;
					ctx->state = no_body ? H1_STATE_DONE : H1_STATE_BODY;
					ctx->total_consumed += 2;
					ctx->current_consumed = 0;

					return H1_NEXT;
				}

				if (found)
					check_end = false;
				else
					goto http1_header_name_end_loop;
			}

			uint8_t *pos_ptr = memchr (start_ptr, ':', len);

			if (pos_ptr)
			{
				size_t memchr_consumed = (size_t) (pos_ptr - start_ptr);

				cur->off += memchr_consumed;
				ctx->current_consumed += memchr_consumed;

				const char *header_name;
				bool is_fragmented = ctx->cur.link != cur->link;
				size_t eff_len = is_fragmented ? memchr_consumed : ctx->current_consumed;

				if (eff_len == 0)
				{
					fh_pr_debug ("Header name is empty");
					return H1_ERR (400);
				}

				if (eff_len > HTTP1_HEADER_NAME_MAX_LEN)
				{
					fh_pr_debug ("Header name is too long");
					return H1_ERR (400);
				}

				if (is_fragmented)
				{
					char *uri_buf = fh_pool_alloc (ctx->stream->pool, eff_len);

					if (!uri_buf)
					{
						fh_pr_debug ("Failed to allocate memory");
						return H1_ERR (500);
					}

					size_t copied = fh_stream_copy (uri_buf, ctx->cur.link, ctx->cur.off, cur->link, cur->off, eff_len);

					if (copied != eff_len)
					{
						fh_pr_debug ("fh_stream_copy() copied %zu/%zu bytes", copied, eff_len);
						return H1_ERR (500);
					}

					fh_pr_debug ("fh_stream_copy() successfully copied %zu bytes", copied);
					header_name = uri_buf;
				}
				else
				{
					header_name = (const char *) (start_ptr);
				}

				ctx->current_header_name = header_name;
				ctx->current_header_name_len = eff_len;

				fh_pr_debug ("Header name: |%.*s|", (int) eff_len, header_name);

				ctx->arg_cur.link = ctx->cur.link = cur->link;
				ctx->arg_cur.off = ctx->cur.off = cur->off + 1; /* For the colon */
				ctx->state = H1_STATE_HEADER_VALUE;
				ctx->total_consumed += ctx->current_consumed + 1;
				ctx->current_consumed = 0;

				return H1_NEXT;
			}
			else
			{
				cur->off += len;
				ctx->current_consumed += len;

				if (len >= 2)
					check_end = false;
			}
		}

	http1_header_name_end_loop:
		if (!cur->link->next)
			break;

		cur->link = cur->link->next;
		cur->off = 0;
	}

	if (ctx->current_consumed > HTTP1_HEADER_NAME_MAX_LEN)
	{
		fh_pr_debug ("Header name is too long");
		return H1_ERR (400);
	}

	return H1_RECV;
}

static bool
fh_http1_populate_attrs (const struct fh_header *header, struct fh_request *request)
{
	if (!strncasecmp (header->name, "Content-Length", header->name_len))
	{
		uint64_t content_length = strntoull (header->value, header->value_len, 10);

		if (errno == EINVAL)
		{
			fh_pr_debug ("Invalid Content-Length header value: |%.*s|", (int) header->value_len, header->value);
			return false;
		}

		request->content_length = content_length;
	}
	else if (!strncasecmp (header->name, "Transfer-Encoding", header->name_len))
	{
		if (!strncasecmp (header->value, "chunked", header->value_len))
		{
			request->transfer_encoding = FH_ENCODING_CHUNKED;
		}
		else
		{
			fh_pr_debug ("Invalid Transfer-Encoding header value: |%.*s|", (int) header->value_len, header->value);
			return false;
		}
	}

	return true;
}

static unsigned int
fh_http1_parse_header_value (struct fh_http1_ctx *ctx)
{
	struct fh_http1_cursor *cur = &ctx->arg_cur;

	while (cur->link && ctx->current_consumed <= HTTP1_HEADER_VALUE_MAX_LEN + 1)
	{
		struct fh_buf *buf = cur->link->buf;

		if (cur->off < buf->len)
		{
			size_t len = buf->len - cur->off;
			uint8_t *start_ptr = buf->data + cur->off;
			uint8_t *pos_ptr = memchr (start_ptr, '\n', len);

			if (pos_ptr)
			{
				size_t memchr_consumed = (size_t) (pos_ptr - start_ptr);

				cur->off += memchr_consumed;
				ctx->current_consumed += memchr_consumed;

				const char *header_value;
				bool is_fragmented = ctx->cur.link != cur->link;
				size_t eff_len = is_fragmented ? memchr_consumed : ctx->current_consumed;

				if (eff_len == 0)
				{
					fh_pr_debug ("Header value is empty");
					return H1_ERR (400);
				}

				if (*(pos_ptr - 1) != '\r')
				{
					fh_pr_debug ("Header value does not end with \\r\\n");
					return H1_ERR (400);
				}

				eff_len--;

				if (eff_len > HTTP1_HEADER_VALUE_MAX_LEN)
				{
					fh_pr_debug ("Header value is too long");
					return H1_ERR (400);
				}

				if (is_fragmented)
				{
					char *uri_buf = fh_pool_alloc (ctx->stream->pool, eff_len);

					if (!uri_buf)
					{
						fh_pr_debug ("Failed to allocate memory");
						return H1_ERR (500);
					}

					size_t copied = fh_stream_copy (uri_buf, ctx->cur.link, ctx->cur.off, cur->link, cur->off, eff_len);

					if (copied != eff_len)
					{
						fh_pr_debug ("fh_stream_copy() copied %zu/%zu bytes", copied, eff_len);
						return H1_ERR (500);
					}

					fh_pr_debug ("fh_stream_copy() successfully copied %zu bytes", copied);
					header_value = uri_buf;
				}
				else
				{
					header_value = (const char *) (start_ptr);
				}

				size_t trimmed_len = 0;
				const char *trimmed_header_value = str_trim_whitespace (header_value, eff_len, &trimmed_len);
				struct fh_header *header = NULL;

				if (!(header = fh_header_add (ctx->stream->pool, &ctx->request.headers, ctx->current_header_name,
											  ctx->current_header_name_len, trimmed_header_value, trimmed_len)))
				{
					fh_pr_debug ("Failed to add header");
					return H1_ERR (500);
				}

				fh_pr_debug ("Header value: |%.*s|", (int) trimmed_len, trimmed_header_value);

				if (!fh_http1_populate_attrs (header, &ctx->request))
				{
					fh_pr_debug ("Failed to validate header");
					return H1_ERR (500);
				}

				ctx->arg_cur.link = ctx->cur.link = cur->link;
				ctx->arg_cur.off = ctx->cur.off = cur->off + 1; /* For the colon */
				ctx->state = H1_STATE_HEADER_NAME;
				ctx->total_consumed += ctx->current_consumed + 1;
				ctx->current_consumed = 0;

				return H1_NEXT;
			}
			else
			{
				cur->off += len;
				ctx->current_consumed += len;
			}
		}

		if (!cur->link->next)
			break;

		cur->link = cur->link->next;
		cur->off = 0;
	}

	if (ctx->current_consumed > HTTP1_HEADER_VALUE_MAX_LEN + 1)
	{
		fh_pr_debug ("Header value is too long");
		return H1_ERR (400);
	}

	return H1_RECV;
}

static unsigned int
fh_http1_parse_body (struct fh_http1_ctx *ctx)
{
	if (ctx->request.content_length == 0)
		return H1_DONE;

	struct fh_http1_cursor *cur = &ctx->arg_cur;

	if (!ctx->is_streaming_body)
	{
		struct fh_link *current = cur->link;

		if (!current)
			return H1_ERR (500);

		struct fh_buf *buf = current->buf;

		uint8_t *start = buf->data + cur->off;
		size_t len = cur->off < buf->len ? buf->len - cur->off : 0;

		struct fh_buf *new_buf;

		if (len > 0)
			new_buf = fh_stream_add_buf_data (ctx->stream, start, len, len);
		else
			new_buf = fh_stream_alloc_buf_data (ctx->stream, DEFAULT_BUF_SIZE);

		if (!new_buf)
			return H1_ERR (500);

		if (len > 0)
		{
			buf->len -= len;
			ctx->current_consumed += len;
		}
	}

	while (cur->link && ctx->current_consumed <= ctx->request.content_length)
	{
		struct fh_buf *buf = cur->link->buf;

		if (cur->off < buf->len)
		{
			size_t len = buf->len - cur->off;

			if (len > 0)
			{
				cur->off += len;
				ctx->current_consumed += len;
			}
		}

		if (!cur->link->next)
		{
			if (ctx->current_consumed <= ctx->request.content_length)
				return H1_RECV;

			break;
		}

		cur->link = cur->link->next;
		cur->off = 0;
	}

	ctx->stream->tail->is_eos = true;
	fh_stream_print (ctx->stream);
	return H1_DONE;
}

static unsigned int
fh_http1_recv (struct fh_http1_ctx *ctx, struct fh_conn *conn)
{
	uint8_t *ptr;
	size_t readable;
	bool is_allocated = false;
	struct fh_buf *tail_buf = ctx->stream->tail ? ctx->stream->tail->buf : NULL;

	if (tail_buf && tail_buf->len < tail_buf->cap)
	{
		ptr = tail_buf->data + tail_buf->len;
		readable = tail_buf->cap - tail_buf->len;
	}
	else
	{
		ptr = fh_pool_alloc (ctx->stream->pool, DEFAULT_BUF_SIZE);

		if (!ptr)
		{
			fh_pr_debug ("Failed to allocate memory");
			return H1_ERR (500);
		}

		readable = DEFAULT_BUF_SIZE;
		is_allocated = true;
	}

	ssize_t bytes_read = recv (conn->client_sockfd, ptr, readable, 0);

	if (bytes_read < 0)
	{
		fh_pool_undo_last_alloc (ctx->stream->pool, DEFAULT_BUF_SIZE);

		if (would_block ())
		{
			fh_pr_debug ("recv would block");
			return H1_RET (true);
		}

		fh_pr_debug ("recv error: %s", strerror (errno));
		return H1_ERR (0);
	}
	else if (bytes_read == 0)
	{
		fh_pool_undo_last_alloc (ctx->stream->pool, DEFAULT_BUF_SIZE);
		fh_pr_debug ("recv error: possible HUP: %s", strerror (errno));
		return H1_ERR (0);
	}

	if (is_allocated)
	{
		if (!fh_stream_add_buf_data (ctx->stream, ptr, (size_t) bytes_read, DEFAULT_BUF_SIZE))
		{
			fh_pr_debug ("Memory allocation failed");
			return H1_ERR (500);
		}

		if (!tail_buf)
		{
			ctx->stream->tail->is_start = true;
			ctx->cur.link = ctx->arg_cur.link = ctx->stream->tail;
			ctx->cur.off = ctx->arg_cur.off = 0;
		}

		fh_pr_debug ("Allocated new buffer of size %zu bytes", DEFAULT_BUF_SIZE);
	}
	else
	{
		tail_buf->len += (size_t) bytes_read;
	}

	fh_pr_debug ("Read %zu bytes", (size_t) bytes_read);

	ctx->state = ctx->prev_state;
	return H1_NEXT;
}

bool
fh_http1_parse (struct fh_http1_ctx *ctx, struct fh_conn *conn)
{
	for (;;)
	{
		unsigned int ret = 0;

		switch (ctx->state)
		{
			case H1_STATE_METHOD:
				ret = fh_http1_parse_method (ctx);
				break;

			case H1_STATE_URI:
				ret = fh_http1_parse_uri (ctx);
				break;

			case H1_STATE_VERSION:
				ret = fh_http1_parse_version (ctx);
				break;

			case H1_STATE_HEADER_NAME:
				ret = fh_http1_parse_header_name (ctx);
				break;

			case H1_STATE_HEADER_VALUE:
				ret = fh_http1_parse_header_value (ctx);
				break;

			case H1_STATE_BODY:
				ret = fh_http1_parse_body (ctx);
				break;

			case H1_STATE_RECV:
				ret = fh_http1_recv (ctx, conn);
				break;

			case H1_STATE_DONE:
				return true;

			case H1_STATE_ERROR:
			default:
				return false;
		}

		if (ret & (1U << 31U))
		{
			ctx->state = H1_STATE_ERROR;
			ctx->suggested_code = ret & 0xFFFF;
			return false;
		}

		if (ret & (1 << 8))
			return (bool) (ret & 0xFF);

		if (ret == H1_RECV)
		{
			ctx->prev_state = ctx->state;
			ctx->state = H1_STATE_RECV;
			continue;
		}

		if (ret == H1_DONE)
		{
			ctx->state = H1_STATE_DONE;
			continue;
		}
	}
}
