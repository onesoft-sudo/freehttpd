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

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define FH_LOG_MODULE_NAME "http1/response"

#include "compat.h"
#include "core/stream.h"
#include "http1.h"
#include "http1_response.h"
#include "log/log.h"
#include "macros.h"
#include "mm/pool.h"
#include "utils/strutils.h"
#include "utils/utils.h"

#define H1_RES_NEXT 0x0
#define H1_RES_ERR 0x1
#define H1_RES_DONE 0x2
#define H1_RES_AGAIN 0x3
#define H1_RES_WRITE(next_state) ((1 << 31) | (next_state))

#define fh_prep_write(ctx, buf, len)                                                                                   \
	(ctx)->buffer = (buf);                                                                                             \
	(ctx)->buffer_len = (len);

static struct fh_header default_headers[] = {
	{ .name = "Server", .name_len = 6, .value = "freehttpd", .value_len = 9 },
	{ .name = "Content-Length", .name_len = 14, .value = "0", .value_len = 1 },
	{ .name = "X-Thank-You", .name_len = 11, .value = "For using freehttpd!", .value_len = 20 },
};

static const size_t default_header_count = sizeof (default_headers) / sizeof (default_headers[0]);
static struct fh_header *default_headers_tail = default_headers + 2;

static size_t default_headers_http_size = 0;

static void __attribute__ ((constructor))
fh_default_headers_init (void)
{
	for (size_t i = 0; i < default_header_count; i++)
	{
		default_headers[i].next = i + 1 < default_header_count ? &default_headers[i + 1] : NULL;
		default_headers_http_size += default_headers[i].name_len + default_headers[i].value_len + 4;
	}
}

static inline char *
fh_gen_res_header (char *h_buf, const char *name, size_t name_len, const char *value, size_t value_len, size_t max_len)
{
    if (max_len > 0 && 4 + name_len + value_len > max_len)
    {
        return NULL;
    }

	memcpy (h_buf, name, name_len);
	h_buf += name_len;
	*h_buf = ':';
	*(h_buf + 1) = ' ';
	h_buf += 2;
	memcpy (h_buf, value, value_len);
	h_buf += value_len;
	*h_buf = '\r';
	*(h_buf + 1) = '\n';
	h_buf += 2;
	return h_buf;
}

struct fh_http1_res_ctx *
fh_http1_res_ctx_create (pool_t *pool)
{
	struct fh_http1_res_ctx *ctx = fh_pool_alloc (pool, sizeof (*ctx) + sizeof (*ctx->response));

	if (!ctx)
		return NULL;

	ctx->response = (struct fh_response *) (ctx + 1);
	ctx->pool = pool;
	ctx->buffer_len = 0;
	ctx->header_buffer = NULL;
	ctx->header_buffer_len = 0;
	ctx->state = FH_RES_STATE_HEADERS;
	ctx->response->headers = NULL;

	return ctx;
}

static unsigned int
fh_res_send_headers (struct fh_http1_res_ctx *ctx, struct fh_conn *conn)
{
	(void) conn;

	struct fh_response *response = ctx->response;
	size_t status_text_len = 0;
	const char *status_text = fh_get_status_text (response->status, &status_text_len);
	const struct fh_headers *headers = response->headers;
	const size_t status_line_len = 8 + 1 + 3 + 1 + status_text_len + 2;
	const size_t total_http1_size = (headers == NULL ? 0 : headers->total_http1_size) + default_headers_http_size;
	const size_t buf_size = ctx->header_buffer_len ? ctx->header_buffer_len : (status_line_len + total_http1_size);
	char *buf = ctx->header_buffer ? (char *) ctx->header_buffer : fh_pool_alloc (ctx->pool, buf_size);

	if (!buf)
		return H1_RES_ERR;

	if (!ctx->header_buffer)
	{
		ctx->header_buffer = (uint8_t *) buf;
		ctx->header_buffer_len = buf_size;
	}

	size_t len = status_line_len + total_http1_size;

	if (snprintf (buf, status_line_len, "HTTP/1.%c %3u %s\r\n", response->protocol == FH_PROTOCOL_HTTP_1_0 ? '0' : '1',
				  response->status, status_text)
		< 0)
		return H1_RES_ERR;

	buf[status_line_len - 1] = '\n';
	char *h_buf = buf + status_line_len;

	if (headers)
		default_headers_tail->next = headers->head;

	for (struct fh_header *h = default_headers; h; h = h->next)
		h_buf = fh_gen_res_header (h_buf, h->name, h->name_len, h->value, h->value_len, 0);

	default_headers_tail->next = NULL;
	assert ((size_t) (h_buf - buf) == buf_size && "Invalid write operations performed");

	if (!buf)
		return H1_RES_ERR;

	fh_prep_write (ctx, (const uint8_t *) buf, len);
	return H1_RES_WRITE (FH_RES_STATE_BODY);
}

static unsigned int
fh_res_send_body (struct fh_http1_res_ctx *ctx, struct fh_conn *conn)
{
	(void) conn;

	fh_prep_write (ctx, (const uint8_t *) "\r\n", 2);
	return H1_RES_WRITE (FH_RES_STATE_DONE);
}

static unsigned int
fh_res_write_data (struct fh_http1_res_ctx *ctx, struct fh_conn *conn)
{
	fd_t sockfd = conn->client_sockfd;

	for (;;)
	{
		ssize_t wrote = write (sockfd, ctx->buffer, ctx->buffer_len);

		if (wrote < 1)
		{
			if (would_interrupt ())
				continue;

			if (would_block ())
				return H1_RES_AGAIN;

			return H1_RES_ERR;
		}

		fh_pr_debug ("Wrote %zu bytes", (size_t) wrote);

		if (((size_t) wrote) >= ctx->buffer_len)
		{
			ctx->buffer_len = 0;
			ctx->buffer = NULL;
			ctx->state = ctx->next_state;
			return H1_RES_NEXT;
		}
		else
		{
			ctx->buffer += (size_t) wrote;
			ctx->buffer_len -= (size_t) wrote;
		}
	}
}

bool
fh_http1_send_response (struct fh_http1_res_ctx *ctx, struct fh_conn *conn)
{
	for (;;)
	{
		unsigned int rc = 0;

		switch (ctx->state)
		{
			case FH_RES_STATE_HEADERS:
				rc = fh_res_send_headers (ctx, conn);
				break;

			case FH_RES_STATE_BODY:
				rc = fh_res_send_body (ctx, conn);
				break;

			case H1_RES_STATE_WRITE:
				rc = fh_res_write_data (ctx, conn);
				break;

			case FH_RES_STATE_DONE:
				return true;

			case FH_RES_STATE_ERROR:
			default:
				return false;
		}

		if (rc == H1_RES_ERR)
		{
			ctx->state = FH_RES_STATE_ERROR;
			return false;
		}

		if (rc == H1_RES_AGAIN)
			return true;

		if (rc >> 31)
		{
			ctx->next_state = rc & 0xFFFF;
			ctx->state = H1_RES_STATE_WRITE;
			continue;
		}

		if (rc == H1_RES_DONE)
		{
			ctx->state = FH_RES_STATE_DONE;
			return true;
		}
	}
}
