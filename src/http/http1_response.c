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
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
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

#ifdef HAVE_RESOURCES
	#include "resources.h"
#endif /* HAVE_RESOURCES */

#define H1_RES_NEXT 0x0
#define H1_RES_ERR 0x1
#define H1_RES_DONE 0x2
#define H1_RES_AGAIN 0x3
#define H1_RES_WRITE(next_state) ((1 << 31) | (next_state))

#define fh_prep_write(ctx, _iov, size, data_size)                              \
	(ctx)->iov = (_iov);                                                       \
	(ctx)->iov_size = (size);                                                  \
	(ctx)->iov_data_size = (data_size);

static char default_date_header_value[64] = { 0 };

static struct fh_header default_headers[] = {
	{
		.name = "Server",
		.name_len = 6,
		.value = "freehttpd",
		.value_len = 9,
	},
	{
		.name = "Date",
		.name_len = 4,
		.value = default_date_header_value,
		.value_len = 29,
	},
	{
		.name = "Connection",
		.name_len = 10,
		.value = "close",
		.value_len = 5,
	},
	{
		.name = "X-Thank-You",
		.name_len = 11,
		.value = "For using freehttpd!",
		.value_len = 20,
	},
};

static const size_t default_header_count
	= sizeof (default_headers) / sizeof (default_headers[0]);
static struct fh_header *default_headers_tail
	= default_headers + (default_header_count - 1);
static size_t default_headers_http_size = 0;
static time_t last_date_header_update_time = 0;

static struct fh_buf default_error_response_buf = {
	.type = FH_BUF_DATA,
};
static struct fh_link default_error_response_link = {
	.buf = &default_error_response_buf,
	.is_eos = true,
	.is_start = false,
	.next = NULL,
};

static void __attribute__ ((constructor))
fh_default_headers_init (void)
{
	for (size_t i = 0; i < default_header_count; i++)
	{
		default_headers[i].next
			= i + 1 < default_header_count ? &default_headers[i + 1] : NULL;
		default_headers_http_size
			+= default_headers[i].name_len + default_headers[i].value_len + 4;
	}
}

__always_inline static inline size_t
fh_add_header_iov (struct iovec *iov, size_t iov_index, const char *name,
				   size_t name_len, const char *value, size_t value_len)
{
	iov[iov_index++] = (struct iovec) {
		.iov_base = (void *) name,
		.iov_len = name_len,
	};

	iov[iov_index++] = (struct iovec) {
		.iov_base = ": ",
		.iov_len = 2,
	};

	iov[iov_index++] = (struct iovec) {
		.iov_base = (void *) value,
		.iov_len = value_len,
	};

	iov[iov_index++] = (struct iovec) {
		.iov_base = "\r\n",
		.iov_len = 2,
	};

	return iov_index;
}

struct fh_http1_res_ctx *
fh_http1_res_ctx_create (pool_t *pool)
{
	struct fh_http1_res_ctx *ctx
		= fh_pool_alloc (pool, sizeof (*ctx) + sizeof (*ctx->response));

	if (!ctx)
		return NULL;

	ctx->response = (struct fh_response *) (ctx + 1);
	ctx->pool = pool;
	ctx->iov = NULL;
	ctx->iov_size = 0;
	ctx->iov_data_size = 0;
	ctx->state = FH_RES_STATE_HEADERS;
	ctx->response->headers = NULL;
	ctx->response->content_length = 0;
	ctx->response->pool = pool;

	return ctx;
}

struct fh_http1_res_ctx *
fh_http1_res_ctx_create_with_response (pool_t *pool,
									   struct fh_response *response)
{
	struct fh_http1_res_ctx *ctx = fh_pool_alloc (pool, sizeof (*ctx));

	if (!ctx)
		return NULL;

	ctx->pool = pool;
	ctx->iov = NULL;
	ctx->iov_size = 0;
	ctx->iov_data_size = 0;
	ctx->state = FH_RES_STATE_HEADERS;
	ctx->response = response;
	ctx->response->pool = pool;

	return ctx;
}

__always_inline static inline void
fh_update_date_header_value (time_t now)
{
	struct tm gmt_tm;
	const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
							 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	const char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

	gmtime_r (&now, &gmt_tm);
	snprintf (default_date_header_value, sizeof (default_date_header_value),
			  "%3s, %02d %3s %04d %02d:%02d:%02d GMT", days[gmt_tm.tm_wday],
			  gmt_tm.tm_mday, months[gmt_tm.tm_mon], gmt_tm.tm_year + 1900,
			  gmt_tm.tm_hour, gmt_tm.tm_min, gmt_tm.tm_sec);
}

__always_inline static inline void
fh_update_static_headers (void)
{
	time_t now = time (NULL);

	if (now != last_date_header_update_time)
	{
		fh_update_date_header_value (now);
		last_date_header_update_time = now;
	}
}

__always_inline static inline bool
fh_add_misc_headers (struct fh_response *response, struct iovec *iov,
					 size_t *iov_index_ptr, bool set_transfer_encoding)
{
	size_t iov_index = *iov_index_ptr;

	if (set_transfer_encoding)
	{
		size_t encoding_len = 0;
		const char *transfer_encoding
			= fh_encoding_to_string (response->encoding, &encoding_len);
		fh_add_header_iov (iov, iov_index, "Transfer-Encoding", 17,
						   transfer_encoding, encoding_len);
		iov_index += 4;
	}
	else
	{
		static char content_length[64] = "0";
		int content_length_len = 1;

		if (response->content_length)
		{
			content_length_len
				= snprintf (content_length, sizeof content_length, "%lu",
							response->content_length);

			if (content_length_len < 0)
				return false;
		}

		fh_add_header_iov (iov, iov_index, "Content-Length", 14, content_length,
						   (size_t) content_length_len);
		iov_index += 4;
	}

	*iov_index_ptr = iov_index;
	return true;
}

__always_inline static inline bool
fh_use_default_error_response (struct fh_http1_res_ctx *ctx,
							   struct fh_conn *conn,
							   struct fh_response *response)
{
	size_t status_text_len = 0;
	size_t description_len = 0;
	const char *status_text
		= fh_get_status_text (response->status, &status_text_len);
	const char *description
		= fh_get_status_description (response->status, &description_len);
	uint16_t port = conn->extra->port;
	const char *host = conn->extra->host;
	size_t host_len = conn->extra->host_len;
	size_t buf_len = resource_error_html_len - 16 + (status_text_len * 2)
					 + (3 * 2) + description_len
					 + (port < 10	   ? 1
						: port < 100   ? 2
						: port < 1000  ? 3
						: port < 10000 ? 4
									   : 5)
					 + host_len;
	char *data = fh_pool_alloc (ctx->pool, buf_len + 1);

	if (!data)
		return false;

	if (snprintf (data, buf_len + 1, resource_error_html, response->status,
				  status_text, response->status, status_text, description,
				  (int) host_len, host, port)
		< 0)
		return false;

	default_error_response_buf.attrs.mem.data = (uint8_t *) data;
	default_error_response_buf.attrs.mem.len = buf_len;
	default_error_response_buf.attrs.mem.cap = buf_len;
	response->content_length = buf_len;

	return true;
}

static unsigned int
fh_res_send_headers (struct fh_http1_res_ctx *ctx, struct fh_conn *conn)
{
	(void) conn;

	struct fh_response *response = ctx->response;
	struct fh_headers *headers = response->headers;
	const bool set_transfer_encoding = response->encoding != FH_ENCODING_PLAIN;
	const size_t generated_header_count = 1 + (set_transfer_encoding ? 1 : 0);
	const size_t header_count = (headers ? headers->count : 0)
								+ default_header_count + generated_header_count;
	size_t status_text_len = 0;
	const char *status_text
		= fh_get_status_text (response->status, &status_text_len);
	const size_t status_line_len = 8 + 1 + 3 + 1 + status_text_len + 2;
	const size_t iov_count = (4 * header_count) + 2;
	size_t iov_index = 0;
	struct iovec *iov = fh_pool_alloc (
		ctx->pool, (sizeof (struct iovec) * iov_count) + status_line_len + 1);

	if (!iov)
		return H1_RES_ERR;

	const size_t total_data_size = status_line_len + default_headers_http_size
								   + (headers ? headers->total_http1_size : 0)
								   + 2;
	char *status_line_buf = (char *) (iov + iov_count);

	if (snprintf (status_line_buf, status_line_len + 1, "HTTP/1.%c %3u %s\r\n",
				  response->protocol == FH_PROTOCOL_HTTP_1_0 ? '0' : '1',
				  response->status, status_text)
		< 0)
		return H1_RES_ERR;

	iov[iov_index++] = (struct iovec) {
		.iov_base = status_line_buf,
		.iov_len = status_line_len,
	};

	if (response->use_default_error_response)
	{
		if (!fh_use_default_error_response (ctx, conn, response))
			return H1_RES_ERR;
	}

	fh_update_static_headers ();

	if (headers)
		default_headers_tail->next = headers->head;

	for (struct fh_header *h = default_headers; h; h = h->next, iov_index += 4)
	{
		fh_add_header_iov (iov, iov_index, h->name, h->name_len, h->value,
						   h->value_len);
	}

	if (!fh_add_misc_headers (response, iov, &iov_index, set_transfer_encoding))
		return H1_RES_ERR;

	iov[iov_index++] = (struct iovec) {
		.iov_base = "\r\n",
		.iov_len = 2,
	};

	fh_prep_write (ctx, iov, iov_count, total_data_size);
	return H1_RES_WRITE (FH_RES_STATE_BODY);
}

static unsigned int
fh_res_send_body (struct fh_http1_res_ctx *ctx, struct fh_conn *conn)
{
	fd_t sockfd = conn->client_sockfd;
	struct fh_response *response = ctx->response;

	if (!response->use_default_error_response && !response->content_length)
		return H1_RES_DONE;

	struct fh_link *link = response->use_default_error_response
							   ? &default_error_response_link
							   : response->body_start;

	if (!link)
	{
		errno = EAGAIN;
		return H1_RES_AGAIN;
	}

	struct iovec iov[__IOV_MAX];
	size_t iov_count = 0, iov_off = 0, iov_total_size = 0;
	size_t file_count = 0;
	uint8_t last_buf_type = FH_BUF_DATA;

	while (link || iov_count > 0 || file_count > 0)
	{
		const struct fh_buf *buf = link ? link->buf : NULL;
		bool buf_type_mismatch
			= link && link->next ? last_buf_type != buf->type : true;

		switch (buf->type)
		{
			case FH_BUF_DATA:
				iov[iov_count++] = (struct iovec) {
					.iov_base = buf->attrs.mem.data,
					.iov_len = buf->attrs.mem.len,
				};

				break;

			case FH_BUF_FILE:
				file_count++;
				break;
		}

		if ((buf_type_mismatch && iov_count > 0) || iov_count >= __IOV_MAX - 1)
		{
			for (;;)
			{
				ssize_t wrote = writev (sockfd, iov + iov_off, iov_count);

				if (wrote < 1)
				{
					if (would_block ())
						return H1_RES_AGAIN;

					return H1_RES_ERR;
				}

				if (((size_t) wrote) >= iov_total_size)
				{
					iov_count = 0;
					iov_off = 0;
					break;
				}
				else
				{
					iov_total_size -= (size_t) wrote;
					size_t size = (size_t) wrote;

					while (size > 0)
					{
						if (iov_count && iov->iov_len < size)
						{
							size -= iov->iov_len;
							iov_off++;
							iov_count--;

							if (response->body_start)
								response->body_start
									= response->body_start->next;
							else
								goto writev_loop_end;

							fh_pr_debug ("Removed 1 body iovec");
							continue;
						}

						if (!iov_count)
							break;

						if (response->body_start)
						{
							struct fh_buf *buf = response->body_start->buf;

							buf->attrs.mem.len -= size;
							buf->attrs.mem.data = (void *) (((char *) buf->attrs.mem.data) + size);
							iov->iov_len = buf->attrs.mem.len;
							iov->iov_base = buf->attrs.mem.data;
						}
						else
						{
							goto writev_loop_end;
						}

						size = 0;
						fh_pr_debug ("Adjusted 1 body iovec");
						break;
					}
				}

				continue;
			writev_loop_end:
				break;
			}
		}
		else if ((buf_type_mismatch && file_count > 0) || file_count >= 63)
		{
			for (size_t i = 0; i < file_count; i++)
			{
				struct fh_buf *buf
					= response->body_start ? response->body_start->buf : NULL;

				if (buf)
				{
					fd_t in_fd = buf->attrs.file.file_fd;

					fh_pr_debug ("Sending fd #%d", in_fd);

					ssize_t sent
						= sendfile64 (sockfd, in_fd, (off64_t *) &buf->attrs.file.file_off,
									  buf->attrs.file.file_len);

					if (sent < 1 || ((size_t) sent) < buf->attrs.file.file_len)
					{
						if (would_block ())
						{
							if (sent)
								buf->attrs.file.file_len -= sent;

							return H1_RES_AGAIN;
						}

						return H1_RES_ERR;
					}

					response->body_start = response->body_start->next;
					fh_pr_debug ("Closed fd #%d", in_fd);
					close (in_fd);
				}
				else
				{
					break;
				}
			}

			file_count = 0;
		}

		last_buf_type = buf->type;

		if (link)
		{
			if (link->is_eos)
				break;

			link = link->next;
		}
	}

	response->body_start = link;

	if (link && link->is_eos)
	{
		response->body_start = NULL;
		return H1_RES_DONE;
	}

	return H1_RES_AGAIN;
}

static unsigned int
fh_res_write_data (struct fh_http1_res_ctx *ctx, struct fh_conn *conn)
{
	fd_t sockfd = conn->client_sockfd;

	for (;;)
	{
		ssize_t wrote = writev (sockfd, ctx->iov, ctx->iov_size);

		if (wrote < 1)
		{
			if (would_interrupt ())
				continue;

			if (would_block ())
				return H1_RES_AGAIN;

			return H1_RES_ERR;
		}

		fh_pr_debug ("Wrote %zu bytes", (size_t) wrote);

		if (((size_t) wrote) >= ctx->iov_data_size)
		{
			ctx->iov_data_size = 0;
			ctx->iov = NULL;
			ctx->state = ctx->next_state;
			return H1_RES_NEXT;
		}
		else
		{
			ctx->iov_data_size -= (size_t) wrote;
			size_t size = (size_t) wrote;

			while (size > 0)
			{
				if (ctx->iov_size && ctx->iov->iov_len < size)
				{
					size -= ctx->iov->iov_len;
					ctx->iov++;
					ctx->iov_size--;
					fh_pr_debug ("Removed 1 iovec");
					continue;
				}

				if (!ctx->iov_size)
					break;

				ctx->iov->iov_len -= size;
				ctx->iov->iov_base
					= (void *) (((char *) ctx->iov->iov_base) + size);
				size = 0;
				fh_pr_debug ("Adjusted 1 iovec");
				break;
			}
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

			case FH_RES_STATE_WRITE:
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
			ctx->state = FH_RES_STATE_WRITE;
			continue;
		}

		if (rc == H1_RES_DONE)
		{
			ctx->state = FH_RES_STATE_DONE;
			return true;
		}
	}
}

void
fh_http1_res_ctx_clean (struct fh_http1_res_ctx *ctx)
{
	struct fh_link *link = ctx->response->body_start;

	while (link)
	{
		if (link->buf->type == FH_BUF_FILE)
		{
			close (link->buf->attrs.file.file_fd);
			fh_pr_debug ("Closed fd %d", link->buf->attrs.file.file_fd);
		}

		link = link->next;
	}
}
